/**
 * 11. Bounded blocking queue
 * Fixed capacity; push blocks when full, pop blocks when empty (condition vars).
 */
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <print>

template <typename T, size_t MaxSize>
class BoundedBlockingQueue {
public:
    void push(T value) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_full_.wait(lock, [this] { return queue_.size() < MaxSize; });
        queue_.push(std::move(value));
        not_empty_.notify_one();
    }

    void pop(T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        not_empty_.wait(lock, [this] { return !queue_.empty(); });
        value = std::move(queue_.front());
        queue_.pop();
        not_full_.notify_one();
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable not_full_;
    std::condition_variable not_empty_;
    std::queue<T> queue_;
};

int main() {
    BoundedBlockingQueue<int, 3> q;
    std::thread producer([&] {
        for (int i = 0; i < 6; ++i) {
            q.push(i);
            std::println("push {}", i);
        }
    });
    std::thread consumer([&] {
        for (int i = 0; i < 6; ++i) {
            int v;
            q.pop(v);
            std::println("pop {}", v);
        }
    });
    producer.join();
    consumer.join();

    // Benchmark: bounded queue throughput (capacity 64)
    const int bench_ops = 200'000;
    BoundedBlockingQueue<int, 64> qb;
    auto t0 = std::chrono::steady_clock::now();
    std::thread pbb([&] { for (int i = 0; i < bench_ops; ++i) qb.push(i); });
    std::thread cbb([&] { int v; for (int i = 0; i < bench_ops; ++i) qb.pop(v); });
    pbb.join();
    cbb.join();
    auto t1 = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::println("Benchmark: {} push+pop in {} us = {:.0f} ops/sec", bench_ops, us, 1e6 * bench_ops / std::max(1.0, static_cast<double>(us)));
    return 0;
}
