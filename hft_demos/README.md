# HFT / Low-Latency C++ Interview Practice

Demo implementations for common HFT C++ interview problems. Each file is self-contained with a `main()` that demonstrates usage and **runs a self-benchmark** (throughput, latency, or ops/sec) at the end.

## Build

Requires **C++23** (for `std::print` / `std::println`). Use CMake or compile directly:

```bash
# CMake (builds all)
mkdir build && cd build && cmake .. && make

# Or compile individually, e.g.:
c++ -std=c++23 -O2 -pthread -o 01_thread_safe_queue 01_thread_safe_queue.cpp
```

**Platform notes:**
- **14_tcp_echo_server_nonblocking.cpp**, **15_event_loop_epoll_kqueue.cpp**: POSIX (Linux/macOS). On macOS use `-DCMAKE_CXX_FLAGS="-std=c++17"`.
- **18_hot_loop_optimization.cpp**: Optional SSE2; use `-msse2` or rely on default for x86_64.

## Demos

| # | File | Description |
|---|------|-------------|
| 1 | `01_thread_safe_queue.cpp` | Thread-safe queue (mutex + condition variable) |
| 2 | `02_lockfree_spsc_ring_buffer.cpp` | Lock-free SPSC ring buffer |
| 3 | `03_thread_pool.cpp` | Thread pool with task submission and futures |
| 4 | `04_reader_writer_lock.cpp` | Reader-writer lock |
| 5 | `05_lockfree_stack.cpp` | Lock-free stack (atomic + CAS) |
| 6 | `06_fixed_size_memory_pool.cpp` | Fixed-size memory pool / allocator |
| 7 | `07_slab_allocator.cpp` | Slab allocator for small objects |
| 8 | `08_lru_cache.cpp` | LRU cache O(1) get/put |
| 9 | `09_rate_limiter_token_bucket.cpp` | Rate limiter (token bucket) |
| 10 | `10_timer_wheel.cpp` | Timer wheel for scheduled tasks |
| 11 | `11_bounded_blocking_queue.cpp` | Bounded blocking queue |
| 12 | `12_waitfree_spsc_queue.cpp` | Wait-free SPSC message queue |
| 13 | `13_cache_friendly_hash_table.cpp` | Cache-friendly hash table (open addressing) |
| 14 | `14_tcp_echo_server_nonblocking.cpp` | TCP echo server (nonblocking sockets) |
| 15 | `15_event_loop_epoll_kqueue.cpp` | Event loop (epoll on Linux, kqueue on macOS/BSD) |
| 16 | `16_async_logger.cpp` | Asynchronous logger (background thread) |
| 17 | `17_false_sharing.cpp` | False sharing detection and fix (cache-line padding) |
| 18 | `18_hot_loop_optimization.cpp` | Hot loop optimization (cache + optional SIMD) |
| 19 | `19_market_data_message_queue.cpp` | Market data message queue (low-latency SPSC) |
| 20 | `20_order_book_matching_engine.cpp` | Simple order book / matching engine |
| 21 | `21_fork_mmap_ipc_sync.cpp` | Fork + mmap shared memory IPC; sync via pipe, 2 atomics, mutex, cond var, semaphore |

## Run

From the `HFT` directory (or `build` if using CMake):

```bash
./01_thread_safe_queue
./02_lockfree_spsc_ring_buffer
# ... etc.
```

TCP echo server (14) and event loop (15) are short-lived; run and optionally connect with `nc localhost 8888` for the echo server.
