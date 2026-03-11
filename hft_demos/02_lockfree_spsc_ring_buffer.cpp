/**
 * 2. Lock-free SPSC (Single Producer Single Consumer) ring buffer
 * No mutex; one writer, one reader. Key for low-latency pipelines.
 */
#include <atomic>
#include <vector>
#include <thread>
#include <chrono>
#include <print>
#include <cassert>

template <typename T, size_t Capacity>
class SPSCRingBuffer {
public:
    SPSCRingBuffer() : buffer_(Capacity) {}

    bool push(T value) {
        size_t w = write_idx_.load(std::memory_order_relaxed);
        size_t next = (w + 1) % Capacity;
        if (next == read_idx_.load(std::memory_order_acquire))
            return false;  // full
        buffer_[w] = std::move(value);
        write_idx_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& value) {
        size_t r = read_idx_.load(std::memory_order_relaxed);
        if (r == write_idx_.load(std::memory_order_acquire))
            return false;  // empty
        value = std::move(buffer_[r]);
        read_idx_.store((r + 1) % Capacity, std::memory_order_release);
        return true;
    }

    bool empty() const {
        return read_idx_.load(std::memory_order_acquire) ==
               write_idx_.load(std::memory_order_acquire);
    }

private:
    std::vector<T> buffer_;
    std::atomic<size_t> write_idx_{0};
    std::atomic<size_t> read_idx_{0};
    static_assert(Capacity >= 2, "Capacity must be >= 2");
};

int main() {
    SPSCRingBuffer<int, 64> rb;
    std::atomic<bool> done{false};

    std::thread producer([&] {
        for (int i = 0; i < 100; ++i) {
            while (!rb.push(i)) { /* spin until space */ }
        }
        done = true;
    });

    int count = 0;
    std::thread consumer([&] {
        int v;
        while (!done || !rb.empty()) {
            if (rb.pop(v)) {
                ++count;
                if (count <= 5 || count >= 98)
                    std::println("pop {}", v);
            }
        }
    });

    producer.join();
    consumer.join();
    std::println("Total consumed: {}", count);

    // Benchmark: SPSC throughput
    const int bench_ops = 2'000'000;
    SPSCRingBuffer<int, 256> rbb;
    std::atomic<bool> doneb{false};
    std::atomic<int> countb{0};
    auto t0 = std::chrono::steady_clock::now();
    std::thread pbb([&] {
        for (int i = 0; i < bench_ops; ++i) while (!rbb.push(i)) {}
        doneb = true;
    });
    std::thread cbb([&] {
        int v;
        while (!doneb || !rbb.empty()) { if (rbb.pop(v)) ++countb; }
    });
    pbb.join();
    cbb.join();
    auto t1 = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::println("Benchmark: {} push+pop in {} us = {:.0f} ops/sec", bench_ops, us, 1e6 * bench_ops / std::max(1.0, static_cast<double>(us)));
    return 0;
}
