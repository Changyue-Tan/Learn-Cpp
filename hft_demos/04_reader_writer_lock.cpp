/**
 * 4. Reader-writer lock
 * Multiple readers OR single writer. Good for shared config / order book snapshots.
 */
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <print>

class ReaderWriterLock {
public:
    void lock_read() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] { return !writing_; });
        ++readers_;
    }

    void unlock_read() {
        std::lock_guard<std::mutex> lock(mutex_);
        --readers_;
        if (readers_ == 0) cond_.notify_all();
    }

    void lock_write() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this] { return !writing_ && readers_ == 0; });
        writing_ = true;
    }

    void unlock_write() {
        std::lock_guard<std::mutex> lock(mutex_);
        writing_ = false;
        cond_.notify_all();
    }

private:
    std::mutex mutex_;
    std::condition_variable cond_;
    int readers_ = 0;
    bool writing_ = false;
};

// Scoped guards
struct ReadGuard {
    ReaderWriterLock& rw;
    ReadGuard(ReaderWriterLock& r) : rw(r) { rw.lock_read(); }
    ~ReadGuard() { rw.unlock_read(); }
};
struct WriteGuard {
    ReaderWriterLock& rw;
    WriteGuard(ReaderWriterLock& r) : rw(r) { rw.lock_write(); }
    ~WriteGuard() { rw.unlock_write(); }
};

int main() {
    ReaderWriterLock rw;
    int shared_value = 0;

    std::thread writer([&] {
        for (int i = 0; i < 5; ++i) {
            WriteGuard g(rw);
            shared_value = i * 10;
            std::println("write {}", shared_value);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    });

    std::thread reader1([&] {
        for (int i = 0; i < 10; ++i) {
            ReadGuard g(rw);
            std::println("r1 read {}", shared_value);
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
    });

    std::thread reader2([&] {
        for (int i = 0; i < 10; ++i) {
            ReadGuard g(rw);
            std::println("r2 read {}", shared_value);
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }
    });

    writer.join();
    reader1.join();
    reader2.join();

    // Benchmark: read vs write throughput
    const int bench_ops = 500'000;
    int val = 0;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < bench_ops; ++i) {
        ReadGuard g(rw);
        val += shared_value;
    }
    auto t1 = std::chrono::steady_clock::now();
    for (int i = 0; i < bench_ops / 10; ++i) {
        WriteGuard g(rw);
        shared_value = i;
    }
    auto t2 = std::chrono::steady_clock::now();
    auto us_r = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    auto us_w = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
    std::println("Benchmark: {} reads in {} us = {:.0f} read/sec; {} writes in {} us = {:.0f} write/sec",
                 bench_ops, us_r, 1e6 * bench_ops / std::max(1.0, static_cast<double>(us_r)),
                 bench_ops / 10, us_w, 1e6 * (bench_ops / 10) / std::max(1.0, static_cast<double>(us_w)));
    (void)val;
    return 0;
}
