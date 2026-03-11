/**
 * 3. Thread pool with task submission
 * Fixed number of workers; submit tasks (e.g. std::function) and get futures.
 */
#include <vector>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <chrono>
#include <print>

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads) : stop_(false) {
        workers_.reserve(num_threads);
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        cond_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cond_.notify_all();
        for (auto& w : workers_) w.join();
    }

    template <typename F, typename... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<typename std::invoke_result<F, Args...>::type> {
        using return_type = typename std::invoke_result<F, Args...>::type;
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<return_type> result = task->get_future();
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stop_) throw std::runtime_error("submit on stopped pool");
            tasks_.emplace([task]() { (*task)(); });
        }
        cond_.notify_one();
        return result;
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable cond_;
    bool stop_;
};

int main() {
    ThreadPool pool(4);
    std::vector<std::future<int>> futures;
    for (int i = 0; i < 8; ++i) {
        futures.push_back(pool.submit([i]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            return i * i;
        }));
    }
    for (size_t i = 0; i < futures.size(); ++i)
        std::println("task {} result: {}", i, futures[i].get());

    // Benchmark: submit + get throughput (trivial work)
    const int bench_tasks = 100'000;
    auto t0 = std::chrono::steady_clock::now();
    std::vector<std::future<int>> futs;
    futs.reserve(bench_tasks);
    for (int i = 0; i < bench_tasks; ++i)
        futs.push_back(pool.submit([i]() { return i; }));
    for (auto& f : futs) (void)f.get();
    auto t1 = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::println("Benchmark: {} submit+get in {} us = {:.0f} tasks/sec", bench_tasks, us, 1e6 * bench_tasks / std::max(1.0, static_cast<double>(us)));
    return 0;
}
