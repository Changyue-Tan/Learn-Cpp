/**
 * 16. Asynchronous logger with a background thread
 * Log calls enqueue strings; single worker thread flushes to stdout/file.
 */
#include <queue>
#include <string>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <print>

class AsyncLogger {
public:
    AsyncLogger() : done_(false) {
        worker_ = std::thread([this] {
            while (true) {
                std::string msg;
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    cond_.wait(lock, [this] { return done_ || !queue_.empty(); });
                    if (done_ && queue_.empty()) break;
                    if (!queue_.empty()) {
                        msg = std::move(queue_.front());
                        queue_.pop();
                    }
                }
                if (!msg.empty())
                    std::print("{}", msg);
            }
        });
    }

    ~AsyncLogger() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            done_ = true;
            cond_.notify_all();
        }
        worker_.join();
    }

    void log(std::string msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(msg));
        cond_.notify_one();
    }

private:
    std::queue<std::string> queue_;
    std::mutex mutex_;
    std::condition_variable cond_;
    std::atomic<bool> done_;
    std::thread worker_;
};

int main() {
    AsyncLogger logger;
    logger.log("[INFO] start\n");
    logger.log("[INFO] processing\n");
    logger.log("[INFO] done\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Benchmark: log() throughput (enqueue only)
    const int bench_ops = 500'000;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < bench_ops; ++i) logger.log("[INFO] benchmark line\n");
    auto t1 = std::chrono::steady_clock::now();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // let worker drain
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::println("Benchmark: {} log() in {} us = {:.0f} log/sec", bench_ops, us, 1e6 * bench_ops / std::max(1.0, static_cast<double>(us)));
    return 0;
}
