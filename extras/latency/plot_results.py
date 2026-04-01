#!/usr/bin/env python3
"""
Plot stress-test results from stress_tiers.py CSV output.

Reads a results CSV (variant, tier_clients, duration_s, client_sent_msgs,
server_count, server_avg_ns, server_max_ns) and produces:
  - Average latency (ns) vs concurrent clients, per variant
  - Max latency (ns) vs concurrent clients, per variant
  - Throughput (msgs/s) vs concurrent clients, per variant

Usage:
  python3 plot_results.py [results_1234.csv]
  python3 plot_results.py   # uses latest results_*.csv in script dir
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

try:
    import matplotlib
    matplotlib.use("Agg")  # non-interactive backend unless --show
    import matplotlib.pyplot as plt
    import pandas as pd
except ImportError as e:
    print("Error: plot_results.py needs matplotlib and pandas. Install with:", file=sys.stderr)
    print("  pip install matplotlib pandas", file=sys.stderr)
    raise SystemExit(2) from e


# Script directory; CSV is expected here or path given as argument
ROOT = Path(__file__).resolve().parent


def find_latest_results_csv() -> Path | None:
    """Return the most recent results_<timestamp>.csv in ROOT, or None."""
    candidates = sorted(ROOT.glob("results_*.csv"), key=lambda p: p.stat().st_mtime, reverse=True)
    return candidates[0] if candidates else None


def load_csv(path: Path) -> pd.DataFrame:
    """Load CSV and coerce numeric columns; empty server stats become NaN."""
    df = pd.read_csv(path)
    for col in ("tier_clients", "duration_s", "client_sent_msgs", "server_count", "server_avg_ns", "server_max_ns"):
        if col in df.columns:
            df[col] = pd.to_numeric(df[col], errors="coerce")
    return df


def plot_latency_and_throughput(df: pd.DataFrame, out_prefix: Path) -> None:
    """Create three figures: avg latency, max latency, throughput vs tier_clients per variant."""
    variants = df["variant"].unique()
    # Use consistent colors and markers per variant across subplots
    colors = plt.cm.tab10.colors
    markers = "o", "s", "^", "D", "v", "p", "*", "X"
    style = {v: {"color": colors[i % len(colors)], "marker": markers[i % len(markers)]} for i, v in enumerate(variants)}

    # Drop rows where we have no server stats (so lines don't break)
    has_avg = df["server_avg_ns"].notna()
    has_max = df["server_max_ns"].notna()
    has_count = df["server_count"].notna()

    # --- Average latency (ns) vs tier_clients ---
    fig, ax = plt.subplots(figsize=(8, 5))
    for v in variants:
        sub = df[(df["variant"] == v) & has_avg]
        if sub.empty:
            continue
        ax.plot(
            sub["tier_clients"],
            sub["server_avg_ns"],
            label=v,
            **style[v],
            linestyle="-",
            markersize=8,
        )
    ax.set_xlabel("Concurrent clients")
    ax.set_ylabel("Average latency (ns)")
    ax.set_title("Server average latency vs load tier")
    ax.legend()
    ax.grid(True, alpha=0.3)
    ax.set_xscale("linear")
    fig.tight_layout()
    fig.savefig(out_prefix.with_name(out_prefix.name + "_avg_latency.png"), dpi=150)
    plt.close(fig)

    # --- Max latency (ns) vs tier_clients ---
    fig, ax = plt.subplots(figsize=(8, 5))
    for v in variants:
        sub = df[(df["variant"] == v) & has_max]
        if sub.empty:
            continue
        ax.plot(
            sub["tier_clients"],
            sub["server_max_ns"],
            label=v,
            **style[v],
            linestyle="-",
            markersize=8,
        )
    ax.set_xlabel("Concurrent clients")
    ax.set_ylabel("Max latency (ns)")
    ax.set_title("Server max latency vs load tier")
    ax.legend()
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(out_prefix.with_name(out_prefix.name + "_max_latency.png"), dpi=150)
    plt.close(fig)

    # --- Throughput (server_count / duration_s) vs tier_clients ---
    fig, ax = plt.subplots(figsize=(8, 5))
    for v in variants:
        sub = df[(df["variant"] == v) & has_count]
        if sub.empty:
            continue
        throughput = sub["server_count"] / sub["duration_s"]
        ax.plot(
            sub["tier_clients"],
            throughput,
            label=v,
            **style[v],
            linestyle="-",
            markersize=8,
        )
    ax.set_xlabel("Concurrent clients")
    ax.set_ylabel("Throughput (messages/s)")
    ax.set_title("Server throughput vs load tier")
    ax.legend()
    ax.grid(True, alpha=0.3)
    fig.tight_layout()
    fig.savefig(out_prefix.with_name(out_prefix.name + "_throughput.png"), dpi=150)
    plt.close(fig)


def main() -> int:
    ap = argparse.ArgumentParser(
        prog="plot_results.py",
        description="Plot latency and throughput from stress_tiers.py CSV results.",
    )
    ap.add_argument(
        "csv",
        nargs="?",
        default=None,
        help="Path to results CSV (default: latest results_*.csv in script dir)",
    )
    ap.add_argument(
        "-o",
        "--output-prefix",
        default=None,
        help="Output file prefix for PNGs (default: same as CSV stem)",
    )
    ap.add_argument(
        "--show",
        action="store_true",
        help="Show plots interactively instead of only saving",
    )
    args = ap.parse_args()

    if args.csv:
        csv_path = Path(args.csv)
        if not csv_path.is_absolute():
            csv_path = ROOT / args.csv
        if not csv_path.exists():
            print(f"Error: file not found: {csv_path}", file=sys.stderr)
            return 2
    else:
        csv_path = find_latest_results_csv()
        if not csv_path:
            print("Error: no results_*.csv found in script directory.", file=sys.stderr)
            return 2
        print(f"Using latest CSV: {csv_path}", file=sys.stderr)

    df = load_csv(csv_path)
    out_prefix = Path(args.output_prefix) if args.output_prefix else csv_path.with_suffix("")
    if not out_prefix.is_absolute():
        out_prefix = ROOT / out_prefix

    plot_latency_and_throughput(df, out_prefix)

    print(f"Saved: {out_prefix.name}_avg_latency.png, _max_latency.png, _throughput.png", file=sys.stderr)
    if args.show:
        plt.show()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
