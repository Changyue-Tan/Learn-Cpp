# Benchmarks

The Python driver [`../tools/run_cpp_benchmarks.py`](../tools/run_cpp_benchmarks.py) runs built executables from the CMake build directory (default: `out/build/ninja`), parses throughput / latency / speedup lines from **stdout**, and writes:

- `results/latest.csv` — long-form metrics (`executable`, `metric`, `value`, `unit`, …)
- **`results/by_benchmark/<executable>.png`** — **one chart per program** (horizontal bars by metric, color by category; working-set demo uses MiB vs ns/touch lines)

Outputs under `results/` are gitignored.

**Prerequisite:** `cmake --preset ninja && cmake --build --preset ninja` from the repo root.

By default the root super-build configures **Release** (so benchmarks aren’t dominated by `-O0` overhead). If you want a different build type, set it explicitly at configure time.

Use **pixi** (`pixi run bench`) so matplotlib matches [`pixi.toml`](../pixi.toml), or install matplotlib into your own environment.
