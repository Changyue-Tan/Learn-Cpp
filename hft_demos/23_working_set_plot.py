import csv
from pathlib import Path

import matplotlib.pyplot as plt


def load_results(csv_path: Path):
    sizes_mib_normal = []
    ns_normal = []
    sizes_mib_huge = []
    ns_huge = []

    with csv_path.open(newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            kind = row["kind"]
            size_mib = float(row["size_mib"])
            ns = float(row["ns_per_touch"])
            if kind == "normal":
                sizes_mib_normal.append(size_mib)
                ns_normal.append(ns)
            elif kind == "huge":
                sizes_mib_huge.append(size_mib)
                ns_huge.append(ns)

    return (sizes_mib_normal, ns_normal), (sizes_mib_huge, ns_huge)


def main():
    csv_path = Path("hft_demos/23_working_set_results.csv")
    if not csv_path.exists():
        raise SystemExit(
            f"{csv_path} not found. Run the C++ benchmark first "
            "from the project root so it generates this file."
        )

    (sizes_n, ns_n), (sizes_h, ns_h) = load_results(csv_path)

    plt.figure(figsize=(8, 5))
    if sizes_n:
        plt.plot(
            sizes_n,
            ns_n,
            marker="o",
            label="normal pages",
        )
    if sizes_h:
        plt.plot(
            sizes_h,
            ns_h,
            marker="s",
            label="huge pages",
        )

    plt.xlabel("Working set size (MiB)")
    plt.ylabel("ns per page-stride access")
    plt.title("Working-set size vs TLB / huge pages")
    plt.xscale("log", base=2)
    plt.grid(True, which="both", linestyle="--", alpha=0.4)
    plt.legend()
    plt.tight_layout()

    out = Path("23_working_set_results.png")
    plt.savefig(out, dpi=150)
    print(f"Saved plot to {out}")


if __name__ == "__main__":
    main()

