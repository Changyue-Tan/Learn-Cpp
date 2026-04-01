#!/usr/bin/env python3
"""
Stress test "tier" load and compare server-side latency across variants.

This repo's variants print:
  FINAL_STATS count=<n> avg_ns=<ns> max_ns=<ns>

This script:
  - builds v*_*.cpp into ./build/ (if needed)
  - for each variant and tier (concurrent clients), runs a measured phase
  - generates TCP traffic to localhost:9999
  - stops the server (newline on stdin) and parses FINAL_STATS
  - prints a summary table and writes a CSV
"""

from __future__ import annotations

import argparse
import csv
import re
import socket
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path


# -----------------------------------------------------------------------------
# Paths and defaults (all server variants listen on 9999)
# -----------------------------------------------------------------------------
ROOT = Path(__file__).resolve().parent
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 9999  # all variants currently hardcode 9999

# Regexes to parse server stdout: VARIANT=... and FINAL_STATS count=... avg_ns=... max_ns=...
FINAL_RE = re.compile(r"FINAL_STATS\s+count=(\d+)\s+avg_ns=(\d+)\s+max_ns=(\d+)")
VARIANT_RE = re.compile(r"^VARIANT=(.*)\s*$")


@dataclass(frozen=True)
class Result:
    variant: str
    tier_clients: int
    duration_s: float
    sent_msgs: int
    final_count: int | None
    avg_ns: int | None
    max_ns: int | None
    raw_output_tail: str  # last N chars of server stdout if FINAL_STATS was missing


def _log(msg: str) -> None:
    """Print progress to stderr so stdout can be used for machine-readable output."""
    print(msg, file=sys.stderr, flush=True)


def parse_int_list(s: str) -> list[int]:
    out: list[int] = []
    for part in s.split(","):
        part = part.strip()
        if not part:
            continue
        out.append(int(part))
    if not out:
        raise ValueError("empty list")
    return out


def discover_variant_sources(pattern: str) -> list[Path]:
    """Return sorted list of paths under ROOT matching the glob (e.g. v*_*.cpp)."""
    return sorted(ROOT.glob(pattern))


def build_if_needed(src: Path, out_dir: Path) -> Path:
    """Compile C++ source to out_dir/<stem> if missing or older than source."""
    out_dir.mkdir(parents=True, exist_ok=True)
    exe = out_dir / src.stem
    if exe.exists() and exe.stat().st_mtime >= src.stat().st_mtime:
        return exe
    cmd = [
        "g++",
        "-std=c++20",
        "-O2",
        "-pthread",
        "-o",
        str(exe),
        str(src),
    ]
    _log(f"[build] {src.name} -> {exe.relative_to(ROOT)}")
    subprocess.run(cmd, cwd=str(ROOT), check=True)
    return exe


def port_is_in_use(host: str, port: int) -> bool:
    """True if a TCP connection to host:port succeeds (something is listening)."""
    try:
        with socket.create_connection((host, port), timeout=0.15):
            return True
    except OSError:
        return False


def wait_for_listen(host: str, port: int, timeout_s: float) -> None:
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        if port_is_in_use(host, port):
            return
        time.sleep(0.03)
    raise RuntimeError(f"timed out waiting for {host}:{port} to accept connections")


def start_server(exe: Path) -> subprocess.Popen[str]:
    """Start the server binary in ROOT; stdin/stdout captured for shutdown and output."""
    proc = subprocess.Popen(
        [str(exe)],
        cwd=str(ROOT),
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )
    return proc


def stop_server(proc: subprocess.Popen[str], timeout_s: float) -> str:
    """Send newline to stdin (variants block on std::cin.get()), then collect full stdout."""
    try:
        out, _ = proc.communicate(input="\n" if proc.stdin else None, timeout=timeout_s)
        return out or ""
    except subprocess.TimeoutExpired:
        _log("[warn] server didn't exit in time; sending SIGTERM")
        try:
            proc.terminate()
        except Exception:
            pass
        try:
            out, _ = proc.communicate(timeout=2.0)
            return out or ""
        except subprocess.TimeoutExpired:
            _log("[warn] server still alive; sending SIGKILL")
            try:
                proc.kill()
            except Exception:
                pass
            out, _ = proc.communicate(timeout=2.0)
            return out or ""


def parse_server_output(output: str) -> tuple[str | None, int | None, int | None, int | None]:
    """Extract VARIANT= name and FINAL_STATS count, avg_ns, max_ns from server stdout."""
    variant_name = None
    for line in output.splitlines():
        m = VARIANT_RE.match(line)
        if m:
            variant_name = m.group(1).strip()
            break
    m = FINAL_RE.search(output)
    if not m:
        return variant_name, None, None, None
    cnt, avg_ns, max_ns = (int(m.group(1)), int(m.group(2)), int(m.group(3)))
    return variant_name, cnt, avg_ns, max_ns


def client_worker(
    host: str,
    port: int,
    payload: bytes,
    stop_evt: threading.Event,
    sent_counter: list[int],
    idx: int,
    per_client_rate: float | None,
) -> None:
    """One load client: connect, send payload in a loop until stop_evt, record count in sent_counter[idx]."""
    sent = 0
    s: socket.socket | None = None
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        s.connect((host, port))
        s.settimeout(1.0)

        if per_client_rate and per_client_rate > 0:
            period_ns = int(1e9 / per_client_rate)
            next_ns = time.perf_counter_ns()
            while not stop_evt.is_set():
                now = time.perf_counter_ns()
                if now < next_ns:
                    time.sleep((next_ns - now) / 1e9)
                else:
                    next_ns = now
                next_ns += period_ns
                s.sendall(payload)
                sent += 1
        else:
            while not stop_evt.is_set():
                s.sendall(payload)
                sent += 1
    except OSError:
        pass
    finally:
        if s is not None:
            try:
                s.shutdown(socket.SHUT_WR)
                s.close()
            except Exception:
                pass
        sent_counter[idx] = sent


def run_load(
    host: str,
    port: int,
    clients: int,
    duration_s: float,
    payload: bytes,
    per_client_rate: float | None,
) -> int:
    """Spawn `clients` client threads, run for `duration_s` seconds, return total messages sent."""
    stop_evt = threading.Event()
    sent_counter = [0] * clients
    threads: list[threading.Thread] = []

    for i in range(clients):
        t = threading.Thread(
            target=client_worker,
            args=(host, port, payload, stop_evt, sent_counter, i, per_client_rate),
            daemon=True,
        )
        threads.append(t)
        t.start()

    time.sleep(duration_s)
    stop_evt.set()

    for t in threads:
        t.join(timeout=1.0)

    return sum(sent_counter)


def tail(s: str, max_chars: int = 600) -> str:
    """Return last max_chars of string (for error hints when FINAL_STATS is missing)."""
    s = s.strip()
    if len(s) <= max_chars:
        return s
    return s[-max_chars:]


def fmt_ns(ns: int | None) -> str:
    """Format nanoseconds as ms / us / ns for human-readable table output."""
    if ns is None:
        return "-"
    if ns >= 1_000_000:
        return f"{ns/1_000_000:.3f} ms"
    if ns >= 1_000:
        return f"{ns/1_000:.3f} us"
    return f"{ns} ns"


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(
        prog="stress_tiers.py",
        description="Stress test server variants by load tier and compare latency (FINAL_STATS).",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    ap.add_argument(
        "--pattern",
        default="v*_*.cpp",
        help="glob pattern (under personal_projects/) for variant sources",
    )
    ap.add_argument(
        "--tiers",
        default="1,2,4,8,16,32",
        type=parse_int_list,
        help="comma-separated concurrent client tiers",
    )
    ap.add_argument("--host", default=DEFAULT_HOST)
    ap.add_argument("--port", type=int, default=DEFAULT_PORT)
    ap.add_argument("--duration", type=float, default=5.0, help="measured seconds")
    ap.add_argument("--warmup", type=float, default=1.0, help="warmup seconds (discarded)")
    ap.add_argument("--payload-bytes", type=int, default=32, help="TCP payload size")
    ap.add_argument(
        "--per-client-rate",
        type=float,
        default=0.0,
        help="messages/sec per client (0 = tight loop)",
    )
    ap.add_argument(
        "--force",
        action="store_true",
        help="run even if something already listens on host:port",
    )
    ap.add_argument(
        "--csv",
        default="",
        help="output CSV path (default: ./results_<ts>.csv)",
    )
    ap.add_argument(
        "--no-build",
        action="store_true",
        help="do not build; treat pattern matches as executables instead of .cpp",
    )
    ap.add_argument(
        "--shutdown-timeout",
        type=float,
        default=10.0,
        help="seconds to wait for server to exit after newline (increase under heavy load)",
    )
    args = ap.parse_args(argv)

    if args.port != DEFAULT_PORT:
        _log(
            f"[warn] variants currently hardcode port {DEFAULT_PORT}; "
            f"--port={args.port} will only affect clients"
        )

    # Ensure no other process is already bound to the port (would block server start).
    if port_is_in_use(args.host, args.port) and not args.force:
        _log(
            f"[error] {args.host}:{args.port} is already accepting connections. "
            "Stop the running server (or pass --force)."
        )
        return 2

    # Resolve variant sources from glob and build executables.
    matches = discover_variant_sources(args.pattern)
    if not matches:
        _log(f"[error] no matches for pattern {args.pattern!r} under {ROOT}")
        return 2

    build_dir = ROOT / "build"
    executables: list[Path] = []
    if args.no_build:
        executables = [Path(m).resolve() for m in matches]
    else:
        for src in matches:
            if src.suffix != ".cpp":
                continue
            executables.append(build_if_needed(src, build_dir))

    if not executables:
        _log("[error] no executables to run")
        return 2

    payload = b"x" * max(1, int(args.payload_bytes))
    per_client_rate = args.per_client_rate if args.per_client_rate and args.per_client_rate > 0 else None

    # For each variant and each tier: optional warmup, then measured phase; parse FINAL_STATS.
    results: list[Result] = []
    for exe in executables:
        _log(f"[run] variant={exe.name}")
        for tier in args.tiers:
            _log(f"  [tier] clients={tier} warmup={args.warmup}s measure={args.duration}s")

            # Warmup phase (discard stats): start server, run load, stop server.
            if args.warmup and args.warmup > 0:
                proc = start_server(exe)
                try:
                    wait_for_listen(args.host, DEFAULT_PORT, timeout_s=2.0)
                    run_load(args.host, args.port, tier, args.warmup, payload, per_client_rate)
                finally:
                    stop_server(proc, timeout_s=args.shutdown_timeout)
                time.sleep(0.15)

            # Measured phase
            proc = start_server(exe)
            output = ""
            sent_msgs = 0
            try:
                wait_for_listen(args.host, DEFAULT_PORT, timeout_s=2.0)
                sent_msgs = run_load(args.host, args.port, tier, args.duration, payload, per_client_rate)
            finally:
                output = stop_server(proc, timeout_s=args.shutdown_timeout)
            variant_name, final_count, avg_ns, max_ns = parse_server_output(output)
            results.append(
                Result(
                    variant=variant_name or exe.name,
                    tier_clients=tier,
                    duration_s=args.duration,
                    sent_msgs=sent_msgs,
                    final_count=final_count,
                    avg_ns=avg_ns,
                    max_ns=max_ns,
                    raw_output_tail=tail(output),
                )
            )
            time.sleep(0.2)

    # Write CSV and print summary table.
    ts = int(time.time())
    csv_path = Path(args.csv) if args.csv else (ROOT / f"results_{ts}.csv")
    with csv_path.open("w", newline="") as f:
        w = csv.writer(f)
        w.writerow(
            [
                "variant",
                "tier_clients",
                "duration_s",
                "client_sent_msgs",
                "server_count",
                "server_avg_ns",
                "server_max_ns",
            ]
        )
        for r in results:
            w.writerow([r.variant, r.tier_clients, r.duration_s, r.sent_msgs, r.final_count, r.avg_ns, r.max_ns])

    # Pretty-ish console summary
    print()
    print(f"Wrote CSV: {csv_path}")
    print()
    print("variant\tclients\tsent\tcount\tavg\tmax")
    for r in results:
        print(
            f"{r.variant}\t{r.tier_clients}\t{r.sent_msgs}\t"
            f"{r.final_count if r.final_count is not None else '-'}\t"
            f"{fmt_ns(r.avg_ns)}\t{fmt_ns(r.max_ns)}"
        )

    # If any run didn't produce FINAL_STATS, surface a hint.
    missing = [r for r in results if r.final_count is None]
    if missing:
        print()
        print("Some runs did not produce FINAL_STATS. Tail output for the first missing run:")
        print(missing[0].raw_output_tail)

    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))

