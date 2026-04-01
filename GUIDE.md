# Learn-C++ quick guide

Runnable examples for **modern C++ (C++20/23)** and **low-latency / performance** patterns. Build everything from the repo root (see [README.md](README.md)).

## Toolchain quirks

- **C++23** is assumed (`std::print` / `std::println`). GCC **15** is preferred when available (see root `CMakeLists.txt`).
- **Benchmarks build type**: the root CMake defaults to **Release** for Ninja/Make (single-config generators), otherwise many of the “micro” benchmarks are misleading under `-O0`.
- **[`topics/language/mdspan.cpp`](topics/language/mdspan.cpp)** is kept in-tree but **not** built: current libstdc++ may lack `<mdspan>`. Enable it in `CMakeLists.txt` when your standard library supports it.
- **POSIX**: [`topics/io/tcp_echo_server_nonblocking.cpp`](topics/io/tcp_echo_server_nonblocking.cpp), [`topics/io/event_loop_epoll_kqueue.cpp`](topics/io/event_loop_epoll_kqueue.cpp) target Linux/macOS-style APIs.
- **Working-set / TLB demo**: run **`working_set_tlb_huge_pages`** (writes `working_set_results.csv` into your current working directory; the benchmark harness runs it from `topics/microarchitecture/`), then `pixi run plot-working-set` (see [`pixi.toml`](pixi.toml)). The CSV/PNG outputs are intentionally gitignored.

## Python benchmark harness

After building the CMake targets, run [`tools/run_cpp_benchmarks.py`](tools/run_cpp_benchmarks.py) to execute demos that print `Benchmark:` (or other parsed lines), save **`benchmarks/results/latest.csv`**, and generate **one graph per benchmark** under **`benchmarks/results/by_benchmark/`** (including the working-set/TLB demo).

```bash
pixi run bench              # full suite (can take several minutes)
pixi run bench-quick        # small subset
python tools/run_cpp_benchmarks.py --only thread_safe_queue hot_loop_optimization
python tools/run_cpp_benchmarks.py --no-plot   # CSV only
python tools/run_cpp_benchmarks.py --no-plot   # CSV only
```

Most throughput demos print **two** `Benchmark [tag]: … = … ops/sec` lines: a **baseline** (e.g. `malloc`, `mutex_deque`, `std::unordered_map`, sequential loop) and the **demo implementation** (`fixed_pool`, `lockfree_ring`, `open_addressing`, `thread_pool`, …). See [`benchmarks/README.md`](benchmarks/README.md). Demos without a numeric bench line (language tours, `smart_container_demo`, parallel-math apps) are intentionally omitted. Programs that already compared multiple designs in one binary (`false_sharing`, `aos_vs_soa_particles`, `context_switch_threads_vs_processes`, `notes_spsc_bump_vs_double_ring`, `working_set_tlb_huge_pages`) were left as-is; `fork_mmap_ipc_sync` stays a single end-to-end IPC timing.

For practical measurement notes (pinning, repeats, warmup, tail latency), see [`benchmarks/METHODOLOGY.md`](benchmarks/METHODOLOGY.md).

## Learning paths

1. **Language first** — [`topics/language/`](topics/language/): small programs, one feature each (`cpp23_*` targets).
2. **Concurrency** — [`topics/concurrency/`](topics/concurrency/): queues, locks, lock-free, IPC; pair with **`cpp23_spsc_ring_buffer`** for a minimal ring + `std::span` API.
3. **Memory** — [`topics/memory/`](topics/memory/): pools, slab, LRU, smart container.
4. **Microarchitecture** — [`topics/microarchitecture/`](topics/microarchitecture/): do **`false_sharing`** before **`hot_loop_optimization`**; **`aos_vs_soa_particles`** for AoS vs SoA.
5. **I/O & scheduling** — [`topics/io/`](topics/io/): timers, rate limiting, nonblocking TCP, event loop, async logging.
6. **Domain sketches** — [`topics/domain_sketches/`](topics/domain_sketches/): market-data and order-book style code (illustrative, not generic “stdlib quirks”).

## SPSC and rings (related samples)

| Role | CMake target | Source |
|------|----------------|--------|
| Minimal teaching ring + `std::span` | `cpp23_spsc_ring_buffer` | [`topics/language/spsc_ring_buffer.cpp`](topics/language/spsc_ring_buffer.cpp) |
| Commentary: bump vs double-ring (Chinese notes in source) | `notes_spsc_bump_vs_double_ring` | [`topics/concurrency/notes_spsc_bump_vs_double_ring.cpp`](topics/concurrency/notes_spsc_bump_vs_double_ring.cpp) |
| Lock-free SPSC ring (power-of-two) | `lockfree_spsc_ring_buffer` | [`topics/concurrency/lockfree_spsc_ring_buffer.cpp`](topics/concurrency/lockfree_spsc_ring_buffer.cpp) |
| Wait-free SPSC | `waitfree_spsc_queue` | [`topics/concurrency/waitfree_spsc_queue.cpp`](topics/concurrency/waitfree_spsc_queue.cpp) |
| Market-data style queue | `market_data_message_queue` | [`topics/domain_sketches/market_data_message_queue.cpp`](topics/domain_sketches/market_data_message_queue.cpp) |

## Pillar index (targets and sources)

### Language ([`topics/language/`](topics/language/))

| Target | Source | Topic |
|--------|--------|--------|
| `cpp23_print` | `print.cpp` | `std::print` / `println` |
| `cpp23_expected` | `expected.cpp` | `std::expected` |
| `cpp23_range_views` | `range_views.cpp` | `std::ranges` / views |
| `cpp23_concepts` | `concepts.cpp` | concepts |
| `cpp23_atomic_ref` | `atomic_ref.cpp` | `std::atomic_ref` |
| `cpp23_bit` | `bit.cpp` | `<bit>` utilities |
| `cpp23_ref` | `ref.cpp` | references / `std::ref` patterns |
| `cpp23_spsc_ring_buffer` | `spsc_ring_buffer.cpp` | ring buffer + `std::span` |
| *(not built)* | `mdspan.cpp` | `std::mdspan` (toolchain-dependent) |

### Concurrency ([`topics/concurrency/`](topics/concurrency/))

| Target | Source |
|--------|--------|
| `thread_safe_queue` | Mutex + condition variable queue |
| `lockfree_spsc_ring_buffer` | Lock-free SPSC ring |
| `thread_pool` | Thread pool + futures |
| `reader_writer_lock` | Reader–writer lock |
| `lockfree_stack` | Lock-free stack (CAS) |
| `bounded_blocking_queue` | Bounded blocking queue |
| `waitfree_spsc_queue` | Wait-free SPSC queue |
| `fork_mmap_ipc_sync` | Fork + mmap IPC, sync primitives |
| `context_switch_threads_vs_processes` | Threads vs processes |
| `notes_spsc_bump_vs_double_ring` | SPSC design notes + code |
| `parallel_math_async` | `parallel_math/main.cpp` — async math |
| `parallel_math_threads` | `parallel_math/main2.cpp` — threaded math |

### Memory ([`topics/memory/`](topics/memory/))

| Target | Source |
|--------|--------|
| `fixed_size_memory_pool` | Fixed-size pool |
| `slab_allocator` | Slab allocator |
| `lru_cache` | LRU cache O(1) |
| `smart_container_demo` | `smart_container/demo.cpp` |

### Microarchitecture ([`topics/microarchitecture/`](topics/microarchitecture/))

| Target | Source |
|--------|--------|
| `cache_friendly_hash_table` | Open-addressing / cache-friendly hash |
| `false_sharing` | False sharing + padding |
| `hot_loop_optimization` | Hot loop (+ optional SSE2 on x86) |
| `working_set_tlb_huge_pages` | Working set / huge pages benchmark |
| `aos_vs_soa_particles` | `aos_vs_soa_particles.cpp` — SoA vs AoS |

### I/O & scheduling ([`topics/io/`](topics/io/))

| Target | Source |
|--------|--------|
| `rate_limiter_token_bucket` | Token bucket |
| `timer_wheel` | Timer wheel |
| `tcp_echo_server_nonblocking` | Nonblocking TCP echo |
| `event_loop_epoll_kqueue` | epoll / kqueue event loop |
| `async_logger` | Async logger |

### Domain sketches ([`topics/domain_sketches/`](topics/domain_sketches/))

| Target | Source |
|--------|--------|
| `market_data_message_queue` | Market-data queue |
| `order_book_matching_engine` | Simple matching engine |
| `hft_capstone_pipeline` | Mini pipeline: MD → queue → book → signal |

### Appendix ([`topics/appendix/classic_algorithms/`](topics/appendix/classic_algorithms/))

| Target | Role |
|--------|------|
| `classic_algorithms` | Static library: `search.cpp`, `sort.cpp` (link your own driver or use as reference) |

## Reference snippet (not in super-build)

Single-file **kitchen-sink** tour (virtual inheritance, smart pointers, `std::format`, etc.) lives in [`snippets/archive/modern_cpp23_kitchen_sink.cpp`](snippets/archive/modern_cpp23_kitchen_sink.cpp). Prefer the **`cpp23_*`** targets above for focused builds; compile this file by hand when you want the all-in-one tour.

```bash
g++ -std=c++23 -Wall -Wextra -O2 -o modern_kitchen_sink snippets/archive/modern_cpp23_kitchen_sink.cpp
```

## Extras (outside this guide)

[`extras/`](extras/) holds side experiments (e.g. interview prep, latency sweeps). They are **not** wired into the root CMake super-build. See each subfolder for its own build or run instructions. Ignore local `build/` and `.pixi/` directories under `extras/` (not part of the curated guide).
