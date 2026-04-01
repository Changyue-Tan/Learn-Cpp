# Topics

This folder contains the curated “main” set of demos and notes. Each subfolder has its own `README.md`, and most sources map 1:1 to a CMake executable target.

## Index

- [language/](language/) — C++20/23 language & library features (`cpp23_*` targets)
- [concurrency/](concurrency/) — queues, locks, lock-free, IPC, `parallel_math/`
- [memory/](memory/) — allocators, LRU, `smart_container`
- [microarchitecture/](microarchitecture/) — false sharing, hot loops, working set/TLB, AoS vs SoA
- [io/](io/) — timers, rate limiting, nonblocking I/O, event loops, async logging
- [domain_sketches/](domain_sketches/) — market data + order book style sketches
- [appendix/](appendix/) — classic algorithms references (`classic_algorithms` library)

See also:

- Bench harness + plots: [`../benchmarks/README.md`](../benchmarks/README.md)\n- Benchmark methodology notes: [`../benchmarks/METHODOLOGY.md`](../benchmarks/METHODOLOGY.md)\n- Lock-free reasoning notes: [`concurrency/lockfree_notes.md`](concurrency/lockfree_notes.md)\n+\n+For build instructions and a target/source table, see [`../GUIDE.md`](../GUIDE.md).

