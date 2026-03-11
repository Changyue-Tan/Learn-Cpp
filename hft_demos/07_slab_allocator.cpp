/**
 * 7. Slab allocator for small objects
 * Multiple size classes; each class has its own fixed-size pool. Reduces fragmentation.
 */
#include <vector>
#include <memory>
#include <cstddef>
#include <chrono>
#include <print>

template <size_t SlotSize>
class SlabSlotPool {
public:
    static constexpr size_t slot_size = SlotSize;

    union Slot {
        Slot* next;
        alignas(16) char data[SlotSize];
    };

    void* allocate() {
        if (free_ == nullptr) grow();
        Slot* p = free_;
        free_ = free_->next;
        return p;
    }

    void deallocate(void* ptr) {
        if (!ptr) return;
        Slot* p = static_cast<Slot*>(ptr);
        p->next = free_;
        free_ = p;
    }

private:
    void grow() {
        const size_t N = 256;
        auto block = std::make_unique<Slot[]>(N);
        for (size_t i = 0; i < N - 1; ++i) block[i].next = &block[i + 1];
        block[N - 1].next = free_;
        free_ = &block[0];
        blocks_.push_back(std::move(block));
    }

    Slot* free_ = nullptr;
    std::vector<std::unique_ptr<Slot[]>> blocks_;
};

// Simpler single-size-class demo
int main() {
    SlabSlotPool<32> pool32;
    std::vector<void*> ptrs;
    for (int i = 0; i < 5; ++i) {
        void* p = pool32.allocate();
        ptrs.push_back(p);
    }
    for (void* p : ptrs) pool32.deallocate(p);
    std::println("Slab allocator (32-byte slots): alloc/dealloc OK");

    // Benchmark: alloc+dealloc throughput
    const int bench_ops = 1'000'000;
    std::vector<void*> ptrsb(bench_ops);
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < bench_ops; ++i) ptrsb[i] = pool32.allocate();
    for (int i = 0; i < bench_ops; ++i) pool32.deallocate(ptrsb[i]);
    auto t1 = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::println("Benchmark: {} alloc+dealloc in {} us = {:.0f} ops/sec", bench_ops, us, 1e6 * bench_ops / std::max(1.0, static_cast<double>(us)));
    return 0;
}
