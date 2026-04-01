/**
 * 17. Detect and fix false sharing in a multithreaded counter
 * False sharing: threads write to different vars on the same cache line -> cache bounces.
 * Fix: pad or align so each counter sits on its own cache line (e.g. 64 bytes).
 */
#include <atomic>
#include <thread>
#include <chrono>
#include <print>

// BAD: multiple atomics on same cache line -> false sharing
struct BadCounter {
    std::atomic<uint64_t> value{0};
    void inc() { value.fetch_add(1, std::memory_order_relaxed); }
};
struct BadCounters {
    BadCounter a, b, c, d;  // all likely on same cache line
};

// GOOD: each counter on separate cache line (typical 64 bytes)
struct alignas(64) PaddedCounter {
    std::atomic<uint64_t> value{0};
    void inc() { value.fetch_add(1, std::memory_order_relaxed); }
};
struct GoodCounters {
    PaddedCounter a, b, c, d;
};

template <typename T>
void run_bench(const char* name, int iterations) {
    T counters;
    auto start = std::chrono::steady_clock::now();
    std::thread t1([&] { for (int i = 0; i < iterations; ++i) counters.a.inc(); });
    std::thread t2([&] { for (int i = 0; i < iterations; ++i) counters.b.inc(); });
    std::thread t3([&] { for (int i = 0; i < iterations; ++i) counters.c.inc(); });
    std::thread t4([&] { for (int i = 0; i < iterations; ++i) counters.d.inc(); });
    t1.join(); t2.join(); t3.join(); t4.join();
    auto end = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    std::println("{}: {} us for {} incs each", name, us, iterations);
}

int main() {
    const int N = 1000000;
    run_bench<BadCounters>("False sharing (bad)", N);
    run_bench<GoodCounters>("Cache-line padded (good)", N);
    std::println("Benchmark: 4 threads, {} incs each; padded typically 2-5x faster (see us above)", N);
    return 0;
}
