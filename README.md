# Learn-C++

Personal repo: **C++20/23 language features**, **concurrency**, **memory**, **microarchitecture**, and **low-latency-style** demos — organized as a small quick guide.

**Start here:** [GUIDE.md](GUIDE.md) (learning paths, CMake target names, toolchain notes, SPSC cross-links).

## Layout

| Path | Contents |
|------|----------|
| [topics/language/](topics/language/) | One executable per language/stdlib topic (`cpp23_*` in CMake) |
| [topics/concurrency/](topics/concurrency/) | Threading, lock-free, IPC, `parallel_math`, SPSC notes |
| [topics/memory/](topics/memory/) | Allocators, LRU, `smart_container` |
| [topics/microarchitecture/](topics/microarchitecture/) | Cache layout, false sharing, hot loops, TLB/huge pages, AoS vs SoA |
| [topics/io/](topics/io/) | Timers, rate limits, nonblocking I/O, event loops, async logging |
| [topics/domain_sketches/](topics/domain_sketches/) | Domain-style sketches (market data, order book) |
| [topics/appendix/](topics/appendix/) | Classic search/sort sources (`classic_algorithms` static library) |
| [snippets/archive/](snippets/archive/) | Optional all-in-one modern C++ tour (compile ad hoc; see GUIDE) |
| [extras/](extras/) | Side projects — **not** in the root super-build (see GUIDE) |

## Build

Uses [Ninja](https://ninja-build.org/). `compile_commands.json` is emitted under the build directory and copied to the repo root for clangd (it’s gitignored here).

```bash
cmake --preset ninja
cmake --build --preset ninja
```

Without presets:

```bash
cmake -S . -B out/build/ninja -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build out/build/ninja
```

Python: working-set plot and **full demo benchmark suite** via [pixi](https://pixi.sh/) — `pixi run plot-working-set`, `pixi run bench` ([`pixi.toml`](pixi.toml), see [GUIDE.md](GUIDE.md#python-benchmark-harness)).
