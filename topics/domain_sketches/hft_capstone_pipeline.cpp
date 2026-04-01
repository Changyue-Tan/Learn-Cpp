/**
 * Capstone: tiny HFT-style pipeline (market data -> queue -> book -> signal)
 *
 * Goals:
 * - fixed-size messages, no allocations in hot path
 * - SPSC queue with acquire/release correctness
 * - measurable throughput + a basic end-to-end latency estimate
 */
#include <atomic>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <print>
#include <thread>

using Clock = std::chrono::steady_clock;

constexpr size_t CACHE_LINE = 64;

struct alignas(CACHE_LINE) MdUpdate {
    uint32_t seq{};
    uint32_t symbol_id{1};
    int64_t bid_px_ticks{};
    int64_t ask_px_ticks{};
    uint32_t bid_qty{};
    uint32_t ask_qty{};
    uint64_t t_send_ns{};  // producer timestamp (steady_clock)
};
static_assert(std::is_trivially_copyable_v<MdUpdate>);

template <typename T, size_t Capacity>
class alignas(CACHE_LINE) SpscRing {
public:
    static_assert(Capacity >= 2);

    bool push(const T& v) {
        const size_t w = write_.load(std::memory_order_relaxed);
        const size_t next = (w + 1) % Capacity;
        if (next == read_.load(std::memory_order_acquire)) {
            return false; // full
        }
        slots_[w] = v;
        write_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& out) {
        const size_t r = read_.load(std::memory_order_relaxed);
        if (r == write_.load(std::memory_order_acquire)) {
            return false; // empty
        }
        out = slots_[r];
        read_.store((r + 1) % Capacity, std::memory_order_release);
        return true;
    }

private:
    std::array<T, Capacity> slots_{};
    alignas(CACHE_LINE) std::atomic<size_t> write_{0};
    alignas(CACHE_LINE) std::atomic<size_t> read_{0};
};

struct TopOfBook {
    int64_t bid_px_ticks = 0;
    int64_t ask_px_ticks = 0;
    uint32_t bid_qty = 0;
    uint32_t ask_qty = 0;
};

static inline uint64_t now_ns() {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count()
    );
}

int main() {
    constexpr uint32_t symbol = 1;
    constexpr size_t qcap = 4096;
    constexpr uint32_t total = 2'000'000;

    SpscRing<MdUpdate, qcap> q;
    std::atomic<bool> done{false};

    alignas(CACHE_LINE) std::atomic<uint64_t> dropped{0};
    alignas(CACHE_LINE) std::atomic<uint64_t> processed{0};
    alignas(CACHE_LINE) std::atomic<uint64_t> signals{0};

    // Simple “strategy”: fire when spread <= 1 tick and size is decent.
    constexpr int64_t max_spread_ticks = 1;
    constexpr uint32_t min_qty = 100;

    // Consumer: updates top-of-book and computes simple latency stats.
    std::thread consumer([&] {
        TopOfBook book{};
        MdUpdate m{};
        uint64_t sum_lat_ns = 0;
        uint64_t max_lat_ns = 0;

        while (!done.load(std::memory_order_acquire) || q.pop(m)) {
            if (!q.pop(m)) {
                std::this_thread::yield();
                continue;
            }

            const uint64_t t_recv = now_ns();
            const uint64_t lat = (t_recv >= m.t_send_ns) ? (t_recv - m.t_send_ns) : 0;
            sum_lat_ns += lat;
            if (lat > max_lat_ns) max_lat_ns = lat;

            if (m.symbol_id == symbol) {
                book.bid_px_ticks = m.bid_px_ticks;
                book.ask_px_ticks = m.ask_px_ticks;
                book.bid_qty = m.bid_qty;
                book.ask_qty = m.ask_qty;

                const int64_t spread = book.ask_px_ticks - book.bid_px_ticks;
                if (spread <= max_spread_ticks && book.bid_qty >= min_qty && book.ask_qty >= min_qty) {
                    signals.fetch_add(1, std::memory_order_relaxed);
                }
            }

            processed.fetch_add(1, std::memory_order_relaxed);
        }

        const uint64_t n = processed.load(std::memory_order_relaxed);
        if (n > 0) {
            std::println("Latency: avg={} ns, max={} ns ({} msgs)", sum_lat_ns / n, max_lat_ns, n);
        }
    });

    // Producer: generates synthetic MD updates.
    auto t0 = Clock::now();
    std::thread producer([&] {
        for (uint32_t i = 0; i < total; ++i) {
            MdUpdate u{};
            u.seq = i;
            u.symbol_id = symbol;
            u.bid_px_ticks = 100'000 + (i % 4);      // tiny oscillation
            u.ask_px_ticks = u.bid_px_ticks + 1;     // 1 tick spread
            u.bid_qty = 200;
            u.ask_qty = 200;
            u.t_send_ns = now_ns();

            // In real systems you typically *don’t* want unbounded spinning; this is a demo.
            while (!q.push(u)) {
                dropped.fetch_add(1, std::memory_order_relaxed);
                std::this_thread::yield();
            }
        }
        done.store(true, std::memory_order_release);
    });

    producer.join();
    consumer.join();
    auto t1 = Clock::now();

    const double sec = std::chrono::duration<double>(t1 - t0).count();
    const uint64_t n = processed.load(std::memory_order_relaxed);
    const uint64_t d = dropped.load(std::memory_order_relaxed);
    const uint64_t s = signals.load(std::memory_order_relaxed);

    std::println("Processed={} Dropped(spin-full)={} Signals={}", n, d, s);
    if (sec > 0.0) {
        std::println(
            "Benchmark [capstone_pipeline]: {} msgs end-to-end in {:.3f} s = {:.0f} msg/sec",
            n,
            sec,
            n / sec
        );
    }
    return 0;
}

