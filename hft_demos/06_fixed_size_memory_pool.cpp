/**
 * 6. Fixed-size memory pool / allocator
 * Pre-allocate blocks of fixed size; O(1) alloc/free, no fragmentation.
 */
#include <vector>
#include <cstddef>
#include <memory>
#include <chrono>
#include <print>

template <typename T, size_t BlockSize = 256>
class FixedSizePool {
public:
    FixedSizePool() { grow(); }

    T* allocate() {
        if (free_list_ == nullptr) grow();
        Chunk* p = free_list_;
        free_list_ = free_list_->next;
        return reinterpret_cast<T*>(p);
    }

    void deallocate(T* ptr) {
        if (!ptr) return;
        Chunk* p = reinterpret_cast<Chunk*>(ptr);
        p->next = free_list_;
        free_list_ = p;
    }

private:
    union Chunk {
        Chunk* next;
        alignas(T) char storage[sizeof(T)];
    };

    void grow() {
        blocks_.push_back(std::make_unique<Chunk[]>(BlockSize));
        Chunk* base = blocks_.back().get();
        for (size_t i = 0; i < BlockSize - 1; ++i) {
            base[i].next = &base[i + 1];
        }
        base[BlockSize - 1].next = free_list_;
        free_list_ = &base[0];
    }

    std::vector<std::unique_ptr<Chunk[]>> blocks_;
    Chunk* free_list_ = nullptr;
};

// Optional: std::allocator interface
template <typename T, size_t BlockSize = 256>
class PoolAllocator {
public:
    using value_type = T;
    FixedSizePool<T, BlockSize>* pool = nullptr;

    explicit PoolAllocator(FixedSizePool<T, BlockSize>& p) : pool(&p) {}

    T* allocate(size_t n) {
        if (n != 1) throw std::bad_alloc();
        return pool->allocate();
    }
    void deallocate(T* p, size_t n) {
        (void)n;
        pool->deallocate(p);
    }
};

struct Order {
    int id;
    double price;
    int qty;
};

int main() {
    FixedSizePool<Order> pool;
    std::vector<Order*> orders;
    for (int i = 0; i < 10; ++i) {
        Order* o = pool.allocate();
        o->id = i;
        o->price = 100.0 + i * 0.01;
        o->qty = 100;
        orders.push_back(o);
    }
    for (Order* o : orders) {
        std::println("order {} @ {} x {}", o->id, o->price, o->qty);
        pool.deallocate(o);
    }

    // Benchmark: alloc+dealloc throughput
    const int bench_ops = 1'000'000;
    std::vector<Order*> ob(bench_ops);
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < bench_ops; ++i) ob[i] = pool.allocate();
    for (int i = 0; i < bench_ops; ++i) pool.deallocate(ob[i]);
    auto t1 = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::println("Benchmark: {} alloc+dealloc in {} us = {:.0f} ops/sec", bench_ops, us, 1e6 * bench_ops / std::max(1.0, static_cast<double>(us)));
    return 0;
}
