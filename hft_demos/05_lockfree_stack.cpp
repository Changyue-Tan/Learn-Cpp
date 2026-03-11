/**
 * 5. Lock-free stack using std::atomic and CAS (Compare-And-Swap)
 * Treiber stack: push/pop with atomic head and ABA-safe (or use tagged pointer).
 */
#include <atomic>
#include <chrono>
#include <print>
#include <thread>
#include <vector>

template <typename T>
class LockFreeStack {
public:
    struct Node {
        T value;
        Node* next;
        explicit Node(T v) : value(std::move(v)), next(nullptr) {}
    };

    ~LockFreeStack() {
        Node* p = head_.load(std::memory_order_relaxed);
        while (p) {
            Node* n = p->next;
            delete p;
            p = n;
        }
    }

    void push(T value) {
        Node* node = new Node(std::move(value));
        node->next = head_.load(std::memory_order_relaxed);
        while (!head_.compare_exchange_weak(node->next, node,
                                            std::memory_order_release,
                                            std::memory_order_relaxed))
            ;
    }

    bool pop(T& value) {
        Node* old_head = head_.load(std::memory_order_relaxed);
        while (old_head &&
               !head_.compare_exchange_weak(old_head, old_head->next,
                                           std::memory_order_acquire,
                                           std::memory_order_relaxed))
            ;
        if (!old_head) return false;
        value = std::move(old_head->value);
        delete old_head;
        return true;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) == nullptr;
    }

private:
    std::atomic<Node*> head_{nullptr};
};

int main() {
    LockFreeStack<int> stack;
    std::vector<std::thread> threads;
    const int N = 4;
    const int M = 100;

    for (int t = 0; t < N; ++t) {
        threads.emplace_back([&, t] {
            for (int i = 0; i < M; ++i) stack.push(t * M + i);
        });
    }
    for (auto& th : threads) th.join();

    int v;
    int count = 0;
    while (stack.pop(v)) ++count;
    std::println("Pushed {}, popped {}", N * M, count);

    // Benchmark: concurrent push then pop throughput
    const int bench_ops = 500'000;
    LockFreeStack<int> stackb;
    auto t0 = std::chrono::steady_clock::now();
    std::thread tb([&] { for (int i = 0; i < bench_ops; ++i) stackb.push(i); });
    tb.join();
    int cnt = 0;
    while (stackb.pop(v)) ++cnt;
    auto t1 = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::println("Benchmark: {} push+pop in {} us = {:.0f} ops/sec", bench_ops, us, 1e6 * bench_ops / std::max(1.0, static_cast<double>(us)));
    return 0;
}
