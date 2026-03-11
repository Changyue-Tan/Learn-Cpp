/**
 * 19. Market data message queue (low-latency producer/consumer)
 * SPSC ring of fixed-size message slots; cache-line padding to avoid false sharing.
 */
#include <atomic>
#include <vector>
#include <thread>
#include <cstring>
#include <chrono>
#include <print>

constexpr size_t CACHE_LINE = 64;

struct alignas(CACHE_LINE) Msg {
    static constexpr size_t MAX_SIZE = 56;
    uint32_t seq;
    uint32_t symbol_id;
    double bid;
    double ask;
    uint32_t bid_qty;
    uint32_t ask_qty;
    char _pad[MAX_SIZE - 32];
};
static_assert(sizeof(Msg) <= Msg::MAX_SIZE + 8, "Msg size");

template <size_t Capacity>
class MarketDataQueue {
public:
    struct Slot {
        std::atomic<uint32_t> seq{0};
        char payload[sizeof(Msg)];
    };

    bool publish(const Msg& msg) {
        size_t w = write_idx_.load(std::memory_order_relaxed);
        size_t next = (w + 1) % Capacity;
        if (next == read_idx_.load(std::memory_order_acquire))
            return false;
        Slot& s = slots_[w];
        std::memcpy(s.payload, &msg, sizeof(msg));
        s.seq.store(w + 1, std::memory_order_release);
        write_idx_.store(next, std::memory_order_release);
        return true;
    }

    bool consume(Msg& msg) {
        size_t r = read_idx_.load(std::memory_order_relaxed);
        if (r == write_idx_.load(std::memory_order_acquire))
            return false;
        Slot& s = slots_[r];
        std::memcpy(&msg, s.payload, sizeof(msg));
        read_idx_.store((r + 1) % Capacity, std::memory_order_release);
        return true;
    }

private:
    alignas(CACHE_LINE) std::vector<Slot> slots_{Capacity};
    alignas(CACHE_LINE) std::atomic<size_t> write_idx_{0};
    alignas(CACHE_LINE) std::atomic<size_t> read_idx_{0};
};

int main() {
    MarketDataQueue<1024> q;
    std::atomic<bool> done{false};
    std::atomic<uint32_t> received{0};

    std::thread producer([&] {
        for (uint32_t i = 0; i < 1000; ++i) {
            Msg m{};
            m.seq = i;
            m.symbol_id = 1;
            m.bid = 100.0 + i * 0.01;
            m.ask = 100.02 + i * 0.01;
            m.bid_qty = 100;
            m.ask_qty = 100;
            while (!q.publish(m)) {}
        }
        done = true;
    });

    std::thread consumer([&] {
        Msg m;
        while (!done || received.load() < 1000) {
            if (q.consume(m)) {
                received++;
                if (m.seq < 3 || m.seq >= 997)
                    std::println("seq={} bid={}", m.seq, m.bid);
            }
        }
    });

    producer.join();
    consumer.join();
    std::println("Received {} messages.", received.load());

    // Benchmark: publish + consume throughput
    const int bench_ops = 2'000'000;
    MarketDataQueue<1024> qb;
    std::atomic<bool> doneb{false};
    std::atomic<uint32_t> recvb{0};
    auto t0 = std::chrono::steady_clock::now();
    std::thread pbb([&] {
        for (uint32_t i = 0; i < bench_ops; ++i) {
            Msg m{}; m.seq = i; m.symbol_id = 1; m.bid = 100.0; m.ask = 100.02; m.bid_qty = m.ask_qty = 100;
            while (!qb.publish(m)) {}
        }
        doneb = true;
    });
    std::thread cbb([&] {
        Msg m;
        while (!doneb || recvb.load() < bench_ops) { if (qb.consume(m)) ++recvb; }
    });
    pbb.join();
    cbb.join();
    auto t1 = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::println("Benchmark: {} publish+consume in {} us = {:.0f} msg/sec", bench_ops, us, 1e6 * bench_ops / std::max(1.0, static_cast<double>(us)));
    return 0;
}
