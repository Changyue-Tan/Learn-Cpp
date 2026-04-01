#!/usr/bin/env python3
"""
Run built Learn-C++ demos, parse self-reported benchmark lines from stdout,
write CSV, and generate matplotlib plots (one PNG per benchmark under results/by_benchmark/).

Usage (from repo root, after `cmake --build build`):
  python tools/run_cpp_benchmarks.py
  python tools/run_cpp_benchmarks.py --only thread_safe_queue hot_loop_optimization
  python tools/run_cpp_benchmarks.py --no-plot   # CSV only
  pixi run bench
"""

from __future__ import annotations

import argparse
import csv
import re
import subprocess
import sys
import platform
from dataclasses import dataclass, asdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Callable
from math import sqrt

# Repo layout: tools/run_cpp_benchmarks.py -> parent.parent is repo root
REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_BUILD_DIR = REPO_ROOT / "out" / "build" / "ninja"
RESULTS_DIR = REPO_ROOT / "benchmarks" / "results"
MICRO_DIR = REPO_ROOT / "topics" / "microarchitecture"

# Demos that print a numeric benchmark (skip language demos, library-only tools, long non-metric programs).
BENCHMARK_EXECUTABLES: tuple[str, ...] = (
    # concurrency
    "thread_safe_queue",
    "lockfree_spsc_ring_buffer",
    "thread_pool",
    "reader_writer_lock",
    "lockfree_stack",
    "bounded_blocking_queue",
    "waitfree_spsc_queue",
    "fork_mmap_ipc_sync",
    "context_switch_threads_vs_processes",
    "notes_spsc_bump_vs_double_ring",
    # memory
    "fixed_size_memory_pool",
    "slab_allocator",
    "lru_cache",
    # microarchitecture
    "cache_friendly_hash_table",
    "false_sharing",
    "hot_loop_optimization",
    "working_set_tlb_huge_pages",
    "aos_vs_soa_particles",
    # io
    "rate_limiter_token_bucket",
    "timer_wheel",
    "tcp_echo_server_nonblocking",
    "event_loop_epoll_kqueue",
    "async_logger",
    # domain
    "market_data_message_queue",
    "order_book_matching_engine",
    "hft_capstone_pipeline",
)

# exe -> subprocess timeout (seconds)
TIMEOUT_OVERRIDES: dict[str, float] = {
    "context_switch_threads_vs_processes": 400.0,
    "working_set_tlb_huge_pages": 240.0,
    "notes_spsc_bump_vs_double_ring": 180.0,
}

# cwd overrides (default: build dir so relative paths in demos behave)
CWD_OVERRIDES: dict[str, Path] = {
    "working_set_tlb_huge_pages": MICRO_DIR,
}


@dataclass
class BenchRow:
    # Run metadata (repeated on every row for convenience)
    run_ts_utc: str
    git_sha: str
    host: str
    compiler: str
    build_dir: str

    executable: str
    metric: str
    value: float
    unit: str
    category: str  # throughput | latency | speedup | bandwidth
    stderr_snippet: str = ""
    repeat_n: int = 1
    repeat_stdev: float = 0.0


def _run_exe(
    exe: Path,
    cwd: Path,
    timeout: float,
) -> tuple[int, str, str]:
    try:
        p = subprocess.run(
            [str(exe)],
            cwd=str(cwd),
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        return p.returncode, p.stdout, p.stderr
    except subprocess.TimeoutExpired as e:
        out = (e.stdout or "") + "\n[timed out]\n"
        err = e.stderr or ""
        return -1, out, err
    except OSError as e:
        return -1, "", str(e)


# --- Parsers: stdout -> metrics ---

def parse_generic_benchmark_lines(stdout: str, exe: str) -> list[BenchRow]:
    """Benchmark lines with optional [tag]: ... = value unit/sec or = value MB/s."""
    rows: list[BenchRow] = []
    # Throughput-style: ops/sec, read/sec, msg/sec, spin/sec, ...
    pat_sec = re.compile(
        r"Benchmark(?:\s+\[([^\]]+)\])?\s*:\s*.+?=\s*([0-9.eE+-]+)\s*([A-Za-z]+/[A-Za-z]+)"
    )
    for m in pat_sec.finditer(stdout):
        tag = (m.group(1) or "").strip()
        val = float(m.group(2))
        unit = m.group(3)
        if unit == "MB/s":
            continue  # handled by pat_mb (avoid duplicate / wrong category)
        label = unit.replace("/", "_per_").replace("-", "_")
        metric = f"bench_{tag}_{label}" if tag else f"bench_{label}"
        rows.append(BenchRow("", "", "", "", "", exe, metric, val, unit, "throughput"))
    pat_mb = re.compile(
        r"Benchmark(?:\s+\[([^\]]+)\])?\s*:\s*.+?=\s*([0-9.eE+-]+)\s*MB/s"
    )
    for m in pat_mb.finditer(stdout):
        tag = (m.group(1) or "").strip()
        val = float(m.group(2))
        metric = f"bench_{tag}_mb_s" if tag else "bench_mb_s"
        rows.append(BenchRow("", "", "", "", "", exe, metric, val, "MB/s", "bandwidth"))
    return rows


def parse_false_sharing(stdout: str, exe: str) -> list[BenchRow]:
    bad = re.search(r"False sharing \(bad\):\s*(\d+)\s*us", stdout)
    good = re.search(r"Cache-line padded \(good\):\s*(\d+)\s*us", stdout)
    if not bad or not good:
        return []
    ub, ug = float(bad.group(1)), float(good.group(1))
    if ug <= 0:
        return []
    return [
        BenchRow("", "", "", "", "", exe, "bad_total_us", ub, "us", "latency"),
        BenchRow("", "", "", "", "", exe, "good_total_us", ug, "us", "latency"),
        BenchRow("", "", "", "", "", exe, "padded_speedup_vs_bad", ub / ug, "ratio", "speedup"),
    ]


def parse_aos_soa(stdout: str, exe: str) -> list[BenchRow]:
    def g(pat: str) -> float | None:
        m = re.search(pat, stdout)
        return float(m.group(1)) if m else None

    oop_phys = g(r"OOP physics time:\s*([0-9.]+)\s*ms")
    dop_phys = g(r"DOP physics time:\s*([0-9.]+)\s*ms")
    oop_rend = g(r"OOP render time:\s*([0-9.]+)\s*ms")
    dop_rend = g(r"DOP render time:\s*([0-9.]+)\s*ms")
    oop_tot = g(r"OOP total time:\s*([0-9.]+)\s*ms")
    dop_tot = g(r"DOP total time:\s*([0-9.]+)\s*ms")

    rows: list[BenchRow] = []
    for name, val in (
        ("oop_physics_ms", oop_phys),
        ("dop_physics_ms", dop_phys),
        ("oop_render_ms", oop_rend),
        ("dop_render_ms", dop_rend),
        ("oop_total_ms", oop_tot),
        ("dop_total_ms", dop_tot),
    ):
        if val is not None:
            rows.append(BenchRow("", "", "", "", "", exe, name, val, "ms", "latency"))

    if oop_rend is not None and dop_rend is not None and min(oop_rend, dop_rend) > 0:
        rows.append(
                BenchRow("", "", "", "", "", exe, "render_speedup_aos_over_soa", oop_rend / dop_rend, "ratio", "speedup")
        )

    if oop_tot is not None and dop_tot is not None and min(oop_tot, dop_tot) > 0:
        rows.append(
                BenchRow("", "", "", "", "", exe, "total_speedup_aos_over_soa", oop_tot / dop_tot, "ratio", "speedup")
        )

    return rows


def parse_context_switch(stdout: str, exe: str) -> list[BenchRow]:
    rows: list[BenchRow] = []
    tm = re.search(
        r"Threads: total ([0-9.]+) ms,\s*([0-9.]+) ns per switch",
        stdout,
    )
    pm = re.search(
        r"Processes: total ([0-9.]+) ms,\s*([0-9.]+) ns per switch",
        stdout,
    )
    if tm:
        rows.append(
            BenchRow("", "", "", "", "", exe, "threads_ns_per_context_switch", float(tm.group(2)), "ns", "latency")
        )
    if pm:
        rows.append(
            BenchRow("", "", "", "", "", exe, "processes_ns_per_context_switch", float(pm.group(2)), "ns", "latency")
        )
    return rows


def parse_fork_mmap(stdout: str, exe: str) -> list[BenchRow]:
    m = re.search(r"Benchmark: 1 IPC round-trip.*? in (\d+)\s*us", stdout)
    if not m:
        return []
    return [BenchRow("", "", "", "", "", exe, "ipc_round_trip", float(m.group(1)), "us", "latency")]


def parse_notes_mops(stdout: str, exe: str) -> list[BenchRow]:
    """Mops/sec from notes_spsc demo (several scenarios)."""
    rows: list[BenchRow] = []
    # stdout uses unicode arrow from std::format/println
    for m in re.finditer("\u2192\\s*([0-9.]+)\\s*Mops/sec", stdout):
        rows.append(
            BenchRow("", "", "", "", "", exe, f"mops_scenario_{len(rows) + 1}", float(m.group(1)), "Mops/sec", "throughput")
        )
    return rows


def parse_working_set_csv() -> list[BenchRow]:
    """After running working_set_tlb_huge_pages, read CSV beside source."""
    csv_path = MICRO_DIR / "working_set_results.csv"
    if not csv_path.is_file():
        return []
    rows: list[BenchRow] = []
    with csv_path.open(newline="") as f:
        r = csv.DictReader(f)
        for i, row in enumerate(r):
            try:
                mib = float(row["size_mib"])
                ns = float(row["ns_per_touch"])
                kind = row.get("kind", "row")
                rows.append(
                        BenchRow("", "", "", "", "", "working_set_tlb_huge_pages", f"ns_per_touch_{kind}_{mib:g}MiB", ns, "ns/touch", "latency")
                )
            except (KeyError, ValueError):
                continue
    return rows


def _git_sha() -> str:
    try:
        p = subprocess.run(
            ["git", "rev-parse", "HEAD"],
            cwd=str(REPO_ROOT),
            capture_output=True,
            text=True,
            timeout=2.0,
        )
        return (p.stdout or "").strip() if p.returncode == 0 else ""
    except Exception:
        return ""


def _compiler_string() -> str:
    # Best-effort only (portable and cheap).
    for cmd in (["c++", "--version"], ["g++", "--version"], ["clang++", "--version"]):
        try:
            p = subprocess.run(cmd, capture_output=True, text=True, timeout=2.0)
            if p.returncode == 0:
                first = (p.stdout or "").splitlines()[:1]
                return first[0].strip() if first else ""
        except Exception:
            continue
    return ""


def _host_string() -> str:
    try:
        return f"{platform.node()} ({platform.system()} {platform.release()})"
    except Exception:
        return ""


def _attach_run_meta(meta: dict[str, str], rows: list[BenchRow], build_dir: Path, repeat_n: int) -> None:
    for r in rows:
        r.run_ts_utc = meta["run_ts_utc"]
        r.git_sha = meta["git_sha"]
        r.host = meta["host"]
        r.compiler = meta["compiler"]
        r.build_dir = str(build_dir)
        r.repeat_n = repeat_n


def _mean_stdev(xs: list[float]) -> tuple[float, float]:
    if not xs:
        return 0.0, 0.0
    mu = sum(xs) / len(xs)
    if len(xs) < 2:
        return mu, 0.0
    var = sum((x - mu) ** 2 for x in xs) / (len(xs) - 1)
    return mu, sqrt(var)


ParserFn = Callable[[str, str], list[BenchRow]]

SPECIAL_PARSERS: dict[str, ParserFn] = {
    "false_sharing": parse_false_sharing,
    "aos_vs_soa_particles": parse_aos_soa,
    "context_switch_threads_vs_processes": parse_context_switch,
    "fork_mmap_ipc_sync": parse_fork_mmap,
    "notes_spsc_bump_vs_double_ring": parse_notes_mops,
}


def collect_rows(
    build_dir: Path,
    only: set[str] | None,
    repeat: int,
) -> list[BenchRow]:
    meta = {
        "run_ts_utc": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "git_sha": _git_sha(),
        "host": _host_string(),
        "compiler": _compiler_string(),
    }
    all_rows: list[BenchRow] = []
    for exe_name in BENCHMARK_EXECUTABLES:
        if only is not None and exe_name not in only:
            continue
        exe_path = build_dir / exe_name
        if not exe_path.is_file():
            all_rows.append(
                BenchRow(
                    meta["run_ts_utc"],
                    meta["git_sha"],
                    meta["host"],
                    meta["compiler"],
                    str(build_dir),
                    exe_name,
                    "missing_binary",
                    0.0,
                    "n/a",
                    "latency",
                    f"expected {exe_path}",
                )
            )
            continue

        repeat = max(1, int(repeat))
        by_key: dict[tuple[str, str, str], list[float]] = {}
        by_key_meta: dict[tuple[str, str, str], tuple[str, str]] = {}  # (unit, category)
        last_err = ""
        last_out = ""
        last_code = 0

        for _ in range(repeat):
            cwd = CWD_OVERRIDES.get(exe_name, build_dir)
            timeout = TIMEOUT_OVERRIDES.get(exe_name, 120.0)
            code, out, err = _run_exe(exe_path, cwd=cwd, timeout=timeout)
            last_err, last_out, last_code = err, out, code

            if exe_name in SPECIAL_PARSERS:
                metrics = SPECIAL_PARSERS[exe_name](out, exe_name)
            else:
                metrics = parse_generic_benchmark_lines(out, exe_name)

            if exe_name == "working_set_tlb_huge_pages" and code == 0:
                metrics.extend(parse_working_set_csv())

            for m in metrics:
                key = (m.executable, m.metric, m.unit)
                by_key.setdefault(key, []).append(float(m.value))
                by_key_meta.setdefault(key, (m.unit, m.category))

        if not by_key:
            snippet = (last_out + last_err)[-800:]
            all_rows.append(
                BenchRow(
                    meta["run_ts_utc"],
                    meta["git_sha"],
                    meta["host"],
                    meta["compiler"],
                    str(build_dir),
                    exe_name,
                    "parse_failed",
                    float(last_code),
                    "exit_code",
                    "latency",
                    snippet,
                    repeat_n=repeat,
                )
            )
            continue

        rows: list[BenchRow] = []
        for (exe, metric, unit), xs in sorted(by_key.items(), key=lambda kv: kv[0][1]):
            mu, sd = _mean_stdev(xs)
            _unit, cat = by_key_meta[(exe, metric, unit)]
            rows.append(
                BenchRow(
                    meta["run_ts_utc"],
                    meta["git_sha"],
                    meta["host"],
                    meta["compiler"],
                    str(build_dir),
                    exe,
                    metric,
                    mu,
                    _unit,
                    cat,
                    repeat_n=repeat,
                    repeat_stdev=sd,
                )
            )

        if rows:
            rows[0].stderr_snippet = (last_err or "")[-200:]
        all_rows.extend(rows)

    return all_rows


def write_csv(rows: list[BenchRow], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(asdict(rows[0]).keys()) if rows else [])
        if rows:
            w.writeheader()
            for r in rows:
                w.writerow(asdict(r))


def _mpl():
    try:
        import matplotlib

        matplotlib.use("Agg")
        import matplotlib.pyplot as plt

        return plt
    except ImportError:
        return None


_CATEGORY_COLOR = {
    "throughput": "#2e7d32",
    "latency": "#c62828",
    "speedup": "#1565c0",
    "bandwidth": "#6a1b9a",
}


def _plot_working_set_tlb(erows: list[BenchRow], path: Path, plt) -> None:
    """Line chart: MiB vs ns/touch, separate series for normal vs huge pages."""
    import re
    from collections import defaultdict

    pat = re.compile(r"ns_per_touch_(normal|huge)_([0-9.eE+-]+)MiB")
    series: dict[str, list[tuple[float, float]]] = defaultdict(list)
    for r in erows:
        m = pat.match(r.metric)
        if not m:
            continue
        series[m.group(1)].append((float(m.group(2)), float(r.value)))
    fig, ax = plt.subplots(figsize=(10, 5.5))
    if not series:
        ax.set_title("working_set_tlb_huge_pages (no parsed series)")
    else:
        for kind in sorted(series.keys()):
            pts = sorted(series[kind], key=lambda p: p[0])
            xs = [p[0] for p in pts]
            ys = [p[1] for p in pts]
            ax.plot(xs, ys, "o-", label=f"{kind} pages", markersize=6, linewidth=2)
        ax.set_xlabel("Working set size (MiB)")
        ax.set_ylabel("ns per page-stride touch")
        ax.set_xscale("log", base=2)
        ax.set_title("working_set_tlb_huge_pages — TLB / working set")
        ax.legend(loc="best")
        ax.grid(True, linestyle="--", alpha=0.4)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def _plot_single_benchmark(exe: str, erows: list[BenchRow], path: Path, plt) -> None:
    """One figure per executable: tailored layout or horizontal bars by metric."""
    if exe == "working_set_tlb_huge_pages":
        _plot_working_set_tlb(erows, path, plt)
        return

    fig_h = max(4.0, min(16.0, 1.1 + 0.55 * len(erows)))
    fig, ax = plt.subplots(figsize=(9.5, fig_h))
    labels = [f"{r.metric}\n({r.unit})" for r in erows]
    vals = [float(r.value) for r in erows]
    colors = [_CATEGORY_COLOR.get(r.category, "#546e7a") for r in erows]
    y = list(range(len(erows)))
    ax.barh(y, vals, height=0.68, color=colors)
    ax.set_yticks(y, labels, fontsize=9)
    ax.set_xlabel("Measured value (see units on each row)")
    pos = [v for v in vals if v > 0]
    if pos and max(pos) / min(pos) > 40:
        ax.set_xscale("log")
    title = exe.replace("_", " ")
    ax.set_title(title, fontsize=12, fontweight="bold")
    ax.grid(True, axis="x", linestyle="--", alpha=0.35)
    # Legend for colors
    from matplotlib.patches import Patch

    seen = {r.category for r in erows}
    handles = [
        Patch(facecolor=_CATEGORY_COLOR[c], label=c) for c in _CATEGORY_COLOR if c in seen
    ]
    if handles:
        ax.legend(handles=handles, loc="lower right", fontsize=8)
    fig.tight_layout()
    fig.savefig(path, dpi=150)
    plt.close(fig)


def plot_results(rows: list[BenchRow], out_dir: Path) -> bool:
    plt = _mpl()
    if plt is None:
        print("matplotlib not installed; skip plots", file=sys.stderr)
        return False

    from collections import defaultdict

    by_exe: dict[str, list[BenchRow]] = defaultdict(list)
    for r in rows:
        if r.metric in ("parse_failed", "missing_binary"):
            continue
        by_exe[r.executable].append(r)

    per_dir = out_dir / "by_benchmark"
    per_dir.mkdir(parents=True, exist_ok=True)

    # Option C: only per-benchmark plots. If older aggregate files exist from previous runs,
    # remove them so the output directory reflects the current behavior.
    for stale in ("benchmark_overview.png", "benchmark_summary.png", "benchmark_working_set.png"):
        try:
            (out_dir / stale).unlink()
        except FileNotFoundError:
            pass
    n = 0
    for exe in sorted(by_exe.keys()):
        erows = by_exe[exe]
        safe = "".join(c if c.isalnum() or c in "-_" else "_" for c in exe)
        out_path = per_dir / f"{safe}.png"
        _plot_single_benchmark(exe, erows, out_path, plt)
        n += 1

    print(f"Wrote {n} per-benchmark graphs under {per_dir}/", file=sys.stderr)
    return True


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--build-dir",
        type=Path,
        default=DEFAULT_BUILD_DIR,
        help="CMake build directory containing executables",
    )
    ap.add_argument(
        "--out-dir",
        type=Path,
        default=RESULTS_DIR,
        help="Directory for CSV and PNG outputs",
    )
    ap.add_argument(
        "--only",
        nargs="*",
        help="Subset of executable names to run",
    )
    ap.add_argument(
        "--repeat",
        type=int,
        default=1,
        help="Repeat each executable N times and record mean/stdev per metric",
    )
    ap.add_argument("--no-plot", action="store_true", help="Only write CSV")
    args = ap.parse_args()

    only = set(args.only) if args.only else None
    rows = collect_rows(args.build_dir.resolve(), only, repeat=args.repeat)

    ts = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    args.out_dir.mkdir(parents=True, exist_ok=True)
    write_csv(rows, args.out_dir / f"bench_{ts}.csv")
    write_csv(rows, args.out_dir / "latest.csv")

    print(f"Wrote {len(rows)} rows to {args.out_dir / 'latest.csv'}")
    if not args.no_plot:
        ok = plot_results(rows, args.out_dir.resolve())
        if ok:
            od = args.out_dir.resolve()
            print(f"Plots: {od}/by_benchmark/*.png")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
