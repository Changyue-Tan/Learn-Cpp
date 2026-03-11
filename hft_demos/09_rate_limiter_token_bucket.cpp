/**
 * 9. Rate limiter (token bucket)
 * Refill tokens at a fixed rate; consume one per request. Reject when empty.
 */
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>
#include <print>

using namespace std::chrono_literals;

class TokenBucket {
public:
    TokenBucket(double rate_per_sec, double burst_size)
        : rate_(rate_per_sec), capacity_(burst_size), tokens_(burst_size),
          last_refill_(now()) {}

    bool try_consume() {
        std::lock_guard<std::mutex> lock(mutex_);
        refill();
        if (tokens_ >= 1.0) {
            tokens_ -= 1.0;
            return true;
        }
        return false;
    }

    void refill() {
        auto t = now();
        double elapsed = std::chrono::duration<double>(t - last_refill_).count();
        tokens_ = std::min(capacity_, tokens_ + elapsed * rate_);
        last_refill_ = t;
    }

private:
    static std::chrono::steady_clock::time_point now() {
        return std::chrono::steady_clock::now();
    }

    double rate_;
    double capacity_;
    double tokens_;
    std::chrono::steady_clock::time_point last_refill_;
    std::mutex mutex_;
};

int main() {
    TokenBucket bucket(2.0, 5.0);  // 2 tokens/sec, burst 5
    for (int i = 0; i < 10; ++i) {
        bool ok = bucket.try_consume();
        std::println("request {}: {}", i, ok ? "OK" : "rate limited");
        std::this_thread::sleep_for(200ms);
    }

    // Benchmark: try_consume throughput (no refill delay)
    TokenBucket bucket2(1e9, 1e9);  // effectively unlimited
    const int bench_ops = 2'000'000;
    auto t0 = std::chrono::steady_clock::now();
    int ok_count = 0;
    for (int i = 0; i < bench_ops; ++i) if (bucket2.try_consume()) ++ok_count;
    auto t1 = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::println("Benchmark: {} try_consume in {} us = {:.0f} ops/sec ({} accepted)", bench_ops, us, 1e6 * bench_ops / std::max(1.0, static_cast<double>(us)), ok_count);
    return 0;
}
