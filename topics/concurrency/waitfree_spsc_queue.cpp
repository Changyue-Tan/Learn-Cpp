/**
 * 12. Wait-free SPSC message queue
 * SPSC with pre-allocated slots; producer never blocks (drops or overwrites if full).
 * "Wait-free" for producer: one CAS per enqueue; consumer reads at its pace.
 */
#include <atomic>
#include <vector>
#include <deque>
#include <mutex>
#include <thread>
#include <chrono>
#include <print>

template <typename T, size_t MaxSize>
class MutexSPSCDeque {
public:
    bool push(const T& v) {
        std::lock_guard<std::mutex> lk(m_);
        if (q_.size() >= MaxSize) return false;
        q_.push_back(v);
        return true;
    }
    bool pop(T& out) {
        std::lock_guard<std::mutex> lk(m_);
        if (q_.empty()) return false;
        out = q_.front();
        q_.pop_front();
        return true;
    }

private:
    std::mutex m_;
    std::deque<T> q_;
};

template <typename T, size_t Capacity>
class WaitFreeSPSCQueue {
public:
    struct Slot {
        std::atomic<bool> ready{false};
        T value;
    };

    bool enqueue(T value) {
        size_t w = write_idx_.load(std::memory_order_relaxed);
        Slot& s = slots_[w];
        if (s.ready.load(std::memory_order_acquire))
            return false;  // full, drop or spin
        s.value = std::move(value);
        s.ready.store(true, std::memory_order_release);
        write_idx_.store((w + 1) % Capacity, std::memory_order_release);
        return true;
    }

    bool dequeue(T& value) {
        size_t r = read_idx_.load(std::memory_order_relaxed);
        Slot& s = slots_[r];
        if (!s.ready.load(std::memory_order_acquire))
            return false;
        value = std::move(s.value);
        s.ready.store(false, std::memory_order_release);
        read_idx_.store((r + 1) % Capacity, std::memory_order_release);
        return true;
    }

private:
    std::vector<Slot> slots_{Capacity};
    std::atomic<size_t> write_idx_{0};
    std::atomic<size_t> read_idx_{0};
};

int main() {
    WaitFreeSPSCQueue<int, 8> q;
    std::atomic<int> produced(0), consumed(0);

    std::thread producer([&] {
        for (int i = 0; i < 20; ++i) {
            while (!q.enqueue(i)) { /* full */ }
            ++produced;
        }
    });
    std::thread consumer([&] {
        int v;
        while (consumed.load() < 20) {
            if (q.dequeue(v)) {
                std::println("got {}", v);
                ++consumed;
            }
        }
    });

    producer.join();
    consumer.join();
    std::println("produced {} consumed {}", produced.load(), consumed.load());

    // Benchmark: enqueue+dequeue throughput
    const int bench_ops = 2'000'000;

    {
        MutexSPSCDeque<int, 256> mq;
        auto t0 = std::chrono::steady_clock::now();
        std::thread pm([&] {
            for (int i = 0; i < bench_ops; ++i) while (!mq.push(i)) {}
        });
        std::thread cm([&] {
            int v;
            for (int c = 0; c < bench_ops; ) {
                if (mq.pop(v)) ++c;
                else std::this_thread::yield();
            }
        });
        pm.join();
        cm.join();
        auto t1 = std::chrono::steady_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
        std::println("Benchmark [mutex_deque]: {} enq+deq in {} us = {:.0f} ops/sec", bench_ops, us,
                     1e6 * bench_ops / std::max(1.0, static_cast<double>(us)));
    }

    WaitFreeSPSCQueue<int, 256> qb;
    std::atomic<int> prod(0), cons(0);
    auto t0 = std::chrono::steady_clock::now();
    std::thread pbb([&] { for (int i = 0; i < bench_ops; ++i) { while (!qb.enqueue(i)) {} ++prod; } });
    std::thread cbb([&] { int v; while (cons.load() < bench_ops) { if (qb.dequeue(v)) ++cons; } });
    pbb.join();
    cbb.join();
    auto t1 = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::println("Benchmark [waitfree_spsc]: {} enq+deq in {} us = {:.0f} ops/sec", bench_ops, us,
                 1e6 * bench_ops / std::max(1.0, static_cast<double>(us)));
    return 0;
}
