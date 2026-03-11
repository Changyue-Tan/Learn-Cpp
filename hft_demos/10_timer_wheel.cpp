/**
 * 10. Timer wheel for scheduled tasks
 * Fixed number of buckets (slots); each tick advances and runs callbacks due in that slot.
 */
#include <vector>
#include <functional>
#include <chrono>
#include <mutex>
#include <thread>
#include <print>

using namespace std::chrono_literals;

class TimerWheel {
public:
    using Callback = std::function<void()>;
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Ms = std::chrono::milliseconds;

    explicit TimerWheel(size_t num_slots = 64, Ms tick_ms = Ms(10))
        : slots_(num_slots), tick_interval_(tick_ms), current_slot_(0),
          start_(Clock::now()) {}

    void schedule_after(Ms delay, Callback cb) {
        size_t ticks = delay.count() / tick_interval_.count();
        if (ticks == 0) ticks = 1;
        size_t slot = (current_slot_ + ticks) % slots_.size();
        std::lock_guard<std::mutex> lock(mutex_);
        slots_[slot].push_back(std::move(cb));
    }

    void tick() {
        current_slot_ = (current_slot_ + 1) % slots_.size();
        std::vector<Callback> to_run;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            to_run.swap(slots_[current_slot_]);
        }
        for (auto& f : to_run) f();
    }

    size_t current_slot() const { return current_slot_; }

private:
    std::vector<std::vector<Callback>> slots_;
    Ms tick_interval_;
    size_t current_slot_;
    TimePoint start_;
    std::mutex mutex_;
};

int main() {
    TimerWheel wheel(32, std::chrono::milliseconds(50));
    wheel.schedule_after(100ms, [] { std::println("A at 100ms"); });
    wheel.schedule_after(150ms, [] { std::println("B at 150ms"); });
    for (int i = 0; i < 10; ++i) {
        wheel.tick();
        std::this_thread::sleep_for(50ms);
    }

    // Benchmark: schedule + tick throughput (empty ticks)
    const int bench_ticks = 500'000;
    TimerWheel wb(64, 1ms);
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < bench_ticks; ++i) wb.tick();
    auto t1 = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::println("Benchmark: {} ticks in {} us = {:.0f} ticks/sec", bench_ticks, us, 1e6 * bench_ticks / std::max(1.0, static_cast<double>(us)));
    return 0;
}
