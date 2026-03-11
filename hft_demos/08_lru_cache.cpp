/**
 * 8. LRU cache with O(1) get and put
 * Hash map + doubly linked list: map gives O(1) lookup, list gives O(1) reorder.
 */
#include <unordered_map>
#include <list>
#include <optional>
#include <chrono>
#include <print>

template <typename K, typename V>
class LRUCache {
public:
    explicit LRUCache(size_t capacity) : cap_(capacity) {}

    std::optional<V> get(const K& key) {
        auto it = map_.find(key);
        if (it == map_.end()) return std::nullopt;
        list_.splice(list_.begin(), list_, it->second);
        return it->second->second;
    }

    void put(const K& key, const V& value) {
        auto it = map_.find(key);
        if (it != map_.end()) {
            it->second->second = value;
            list_.splice(list_.begin(), list_, it->second);
            return;
        }
        if (list_.size() >= cap_) {
            map_.erase(list_.back().first);
            list_.pop_back();
        }
        list_.emplace_front(key, value);
        map_[key] = list_.begin();
    }

private:
    size_t cap_;
    std::list<std::pair<K, V>> list_;
    std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator> map_;
};

int main() {
    LRUCache<int, std::string> cache(3);
    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");
    std::println("{}", cache.get(1).value_or("?"));  // "one", 1 becomes MRU
    cache.put(4, "four");  // evict 2 (LRU)
    std::println("{}", cache.get(2) ? "hit" : "miss");
    std::println("{}", cache.get(1).value_or("?"));
    std::println("{}", cache.get(4).value_or("?"));

    // Benchmark: get+put throughput
    const int bench_ops = 1'000'000;
    LRUCache<int, int> cb(1024);
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < bench_ops; ++i) {
        cb.put(i % 512, i);
        cb.get(i % 512);
    }
    auto t1 = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::println("Benchmark: {} put+get in {} us = {:.0f} ops/sec", bench_ops, us, 1e6 * bench_ops * 2 / std::max(1.0, static_cast<double>(us)));
    return 0;
}
