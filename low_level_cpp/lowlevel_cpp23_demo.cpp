// ============================================================
// 说明：单 ring + bump allocator vs 双 ring + fixed array
// ============================================================
//
// 1️⃣ Single Ring + Bump Allocator
// - 内存分配：线性 bump，offset++
/*
arena: [obj0, obj1, obj2, ..., objN-1]
offset -> 下一个分配位置
*/
// - pseudo circular：当 offset >= ARENA_CAP 时，reset offset = 0
// - 问题：Producer 可能在 Consumer 还没处理完 obj0 时就覆盖 obj0
//   → 对象生命周期不精确
// - 适用场景：吞吐统计、批量处理、SPSC 单线程
// - 不适用场景：HFT tick、交易订单等必须精确处理每个对象的数据

// 2️⃣ Double Ring + Fixed Array (no pool)
// - 内存分配：固定数组 pool[POOL_SIZE]
// - 对象生命周期精确：
//     Producer 从 free_ring pop → 填充数据 → push used_ring
//     Consumer pop used_ring → 处理 → push 回 free_ring
// - 特点：每个对象只有在 Consumer 处理完后才会被再次分配
// - 适用场景：高性能 HFT、精确对象循环、零拷贝
// - 不需要 pseudo circular，因为 ring 管理单对象生命周期，安全可靠

// 3️⃣ 对比总结
// - Single Ring + Bump Allocator:
//     + 内存连续、批量吞吐高、简单
//     - 对象可能被覆盖，生命周期不精确
// - Double Ring + Fixed Array:
//     + 对象精确循环、零拷贝、安全
//     - 需要维护两个 ring，稍微复杂，原子操作多一点

#include <array>
#include <atomic>
#include <thread>
#include <span>
#include <print>
#include <chrono>
#include <cstdlib>

using namespace std::chrono_literals;

// ============================================================
// 1️⃣ Buffer Object
// ============================================================
struct Buffer {
    char payload[64];
};

// ============================================================
// 2️⃣ SPSC Ring with batch support
// ============================================================
template<typename T, size_t Capacity>
class SPSCRing {
    static_assert((Capacity & (Capacity - 1)) == 0);
private:
    static constexpr size_t mask = Capacity - 1;
    alignas(64) std::array<T, Capacity> buffer_;
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};

public:
    // single push/pop
    bool push(T v) {
        auto head = head_.load(std::memory_order_relaxed);
        auto next = (head + 1) & mask;
        if (next == tail_.load(std::memory_order_acquire)) return false;
        buffer_[head] = v;
        head_.store(next, std::memory_order_release);
        return true;
    }

    bool pop(T& out) {
        auto tail = tail_.load(std::memory_order_relaxed);
        if (tail == head_.load(std::memory_order_acquire)) return false;
        out = buffer_[tail];
        tail_.store((tail + 1) & mask, std::memory_order_release);
        return true;
    }

    // batch pop using span → zero-copy
    size_t pop_batch(std::span<T> out) {
        size_t count = 0;
        while (count < out.size()) {
            T* ptr = &out[count];
            if (!pop(*ptr)) break;
            ++count;
        }
        return count;
    }

    // batch push using span
    size_t push_batch(std::span<T> in) {
        size_t count = 0;
        for (auto& v : in) {
            if (!push(v)) break;
            ++count;
        }
        return count;
    }
};

// ============================================================
// 3️⃣ Benchmark helper
// ============================================================
template<typename F>
void measure(F&& f, const char* name) {
    auto start = std::chrono::high_resolution_clock::now();
    uint64_t processed = f();
    auto end = std::chrono::high_resolution_clock::now();
    double sec = std::chrono::duration<double>(end - start).count();
    std::println("{}: processed {} objects in {:.3f}s → {:.3f} Mops/sec",
                 name, processed, sec, processed / sec / 1'000'000.0);
}

// ============================================================
// 4️⃣ Single Ring + Bump Allocator (pseudo circular)
// ============================================================
uint64_t run_single_ring_bump(bool batch=false) {
    constexpr size_t ARENA_CAP = 1 << 20;
    constexpr size_t RING_SIZE = 1024;
    constexpr size_t BATCH_SIZE = 64;

    static Buffer arena[ARENA_CAP];
    SPSCRing<Buffer*, RING_SIZE> ring;

    std::atomic<size_t> offset{0};
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> processed{0};

    std::jthread producer([&]{
        Buffer* buf;
        std::array<Buffer*, BATCH_SIZE> batch_buf;
        while (!stop.load()) {
            if (!batch) {
                size_t idx = offset.fetch_add(1);
                if (idx >= ARENA_CAP) offset.store(0);
                buf = &arena[idx];
                while (!ring.push(buf)) {}
            } else {
                size_t n = 0;
                for (; n<BATCH_SIZE; ++n) {
                    size_t idx = offset.fetch_add(1);
                    if (idx >= ARENA_CAP) offset.store(0), idx=0;
                    batch_buf[n] = &arena[idx];
                }
                std::span batch_span(batch_buf.data(), n);
                while (ring.push_batch(batch_span) != n) {}
            }
        }
    });

    std::jthread consumer([&]{
        Buffer* buf;
        std::array<Buffer*, BATCH_SIZE> batch_buf;
        while (!stop.load()) {
            if (!batch) {
                if (!ring.pop(buf)) continue;
                processed.fetch_add(1, std::memory_order_relaxed);
            } else {
                std::span batch_span(batch_buf);
                size_t n = ring.pop_batch(batch_span);
                if (n==0) continue;
                processed.fetch_add(n, std::memory_order_relaxed);
            }
        }
    });

    std::this_thread::sleep_for(2s);
    stop = true;
    return processed.load();
}

// ============================================================
// 5️⃣ Double Ring + Fixed Array (no pool)
// ============================================================
uint64_t run_double_ring_fixed(bool batch=false) {
    constexpr size_t POOL_SIZE = 1 << 14;  // 16K
    constexpr size_t RING_SIZE = 1024;
    constexpr size_t BATCH_SIZE = 64;

    static Buffer pool[POOL_SIZE];
    SPSCRing<Buffer*, RING_SIZE> free_ring;
    SPSCRing<Buffer*, RING_SIZE> used_ring;

    for (size_t i=0;i<POOL_SIZE;++i)
        free_ring.push(&pool[i]);

    std::atomic<bool> stop{false};
    std::atomic<uint64_t> processed{0};
    std::array<Buffer*, BATCH_SIZE> batch_buf;

    std::jthread producer([&]{
        Buffer* buf;
        while (!stop.load()) {
            if (!batch) {
                if (!free_ring.pop(buf)) continue;
                while (!used_ring.push(buf)) {}
            } else {
                size_t n=0;
                while (n<BATCH_SIZE) {
                    if (!free_ring.pop(buf)) break;
                    batch_buf[n++] = buf;
                }
                if (n==0) continue;
                std::span span_buf(batch_buf.data(), n);
                while (used_ring.push_batch(span_buf) != n) {}
            }
        }
    });

    std::jthread consumer([&]{
        Buffer* buf;
        while (!stop.load()) {
            if (!batch) {
                if (!used_ring.pop(buf)) continue;
                processed.fetch_add(1,std::memory_order_relaxed);
                while (!free_ring.push(buf)) {}
            } else {
                size_t n = used_ring.pop_batch(std::span(batch_buf));
                if (n==0) continue;
                processed.fetch_add(n,std::memory_order_relaxed);
                std::span span_buf(batch_buf.data(), n);
                while (free_ring.push_batch(span_buf) != n) {}
            }
        }
    });

    std::this_thread::sleep_for(2s);
    stop = true;
    return processed.load();
}

// ============================================================
// 6️⃣ Main
// ============================================================
int main() {
    std::println("=== Single object push/pop ===");
    measure([]{ return run_single_ring_bump(false); }, "Single Ring + Bump Allocator");
    measure([]{ return run_double_ring_fixed(false); }, "Double Ring + Fixed Array");

    std::println("\n=== Batch push/pop using span ===");
    measure([]{ return run_single_ring_bump(true); }, "Single Ring + Bump Allocator (batch)");
    measure([]{ return run_double_ring_fixed(true); }, "Double Ring + Fixed Array (batch)");

    std::println("\nDemo complete.");
}