/**
 * 1. Thread-safe queue (mutex + condition variable)
 * Classic producer-consumer pattern for HFT interview.
 */
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <print>

template <typename T>
class ThreadSafeQueue {
public:
    void push(T value) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(value));
        cond_.notify_one();
    }

    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    bool wait_and_pop(T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] { return !queue_.empty() || done_; });
        if (queue_.empty() && done_) return false;
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void set_done() {
        std::lock_guard<std::mutex> lock(mutex_);
        done_ = true;
        cond_.notify_all();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cond_;
    std::queue<T> queue_;
    bool done_ = false;
};

int main() {
    ThreadSafeQueue<int> q;
    std::atomic<bool> stop{false};

    std::thread producer([&] {
        for (int i = 0; i < 10; ++i) {
            q.push(i);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        q.set_done();
    });

    std::thread consumer([&] {
        int v;
        while (q.wait_and_pop(v)) {
            std::println("consumed {}", v);
        }
    });

    producer.join();
    consumer.join();
    std::println("Done.");

    // Benchmark: push+pop throughput
    const int bench_ops = 500'000;
    ThreadSafeQueue<int> qb;
    std::atomic<bool> doneb{false};
    std::atomic<int> consumed{0};
    auto t0 = std::chrono::steady_clock::now();
    std::thread pb([&] {
        for (int i = 0; i < bench_ops; ++i) qb.push(i);
        qb.set_done();
    });
    std::thread cb([&] {
        int v;
        while (qb.wait_and_pop(v)) ++consumed;
    });
    pb.join();
    cb.join();
    auto t1 = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::println("Benchmark: {} push+pop in {} us = {:.0f} ops/sec", bench_ops, us, 1e6 * bench_ops / std::max(1.0, static_cast<double>(us)));
    return 0;
}
