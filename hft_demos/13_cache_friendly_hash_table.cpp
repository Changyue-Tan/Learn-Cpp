/**
 * 13. Cache-friendly hash table (open addressing)
 * Single contiguous array; linear probing. Good cache line utilization.
 */
#include <vector>
#include <optional>
#include <functional>
#include <chrono>
#include <print>

template <typename K, typename V, typename Hash = std::hash<K>>
class OpenAddressingHashTable {
public:
    explicit OpenAddressingHashTable(size_t capacity = 16) : table_(capacity), size_(0) {}

    bool insert(K key, V value) {
        if (size_ * 2 >= table_.size()) rehash();
        size_t i = index(key);
        for (size_t j = 0; j < table_.size(); ++j) {
            size_t idx = (i + j) % table_.size();
            if (!table_[idx].has_value()) {
                table_[idx] = std::pair<K, V>(std::move(key), std::move(value));
                ++size_;
                return true;
            }
            if (table_[idx]->first == key) {
                table_[idx]->second = std::move(value);
                return true;
            }
        }
        rehash();
        return insert(std::move(key), std::move(value));
    }

    std::optional<V> find(const K& key) const {
        size_t i = index(key);
        for (size_t j = 0; j < table_.size(); ++j) {
            size_t idx = (i + j) % table_.size();
            if (!table_[idx].has_value()) return std::nullopt;
            if (table_[idx]->first == key) return table_[idx]->second;
        }
        return std::nullopt;
    }

    size_t size() const { return size_; }

private:
    size_t index(const K& key) const { return hasher_(key) % table_.size(); }

    void rehash() {
        std::vector<std::optional<std::pair<K, V>>> old = std::move(table_);
        table_.resize(old.size() * 2);
        size_ = 0;
        for (auto& cell : old) {
            if (cell.has_value())
                insert(std::move(cell->first), std::move(cell->second));
        }
    }

    std::vector<std::optional<std::pair<K, V>>> table_;
    size_t size_;
    Hash hasher_;
};

int main() {
    OpenAddressingHashTable<int, std::string> map;
    map.insert(1, "one");
    map.insert(2, "two");
    map.insert(17, "seventeen");  // collision with 1 if cap 16
    std::println("1 => {}", map.find(1).value_or("?"));
    std::println("2 => {}", map.find(2).value_or("?"));
    std::println("17 => {}", map.find(17).value_or("?"));
    std::println("size {}", map.size());

    // Benchmark: insert + find throughput
    const int bench_ops = 500'000;
    OpenAddressingHashTable<int, int> mb(65536);
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < bench_ops; ++i) mb.insert(i, i * 2);
    for (int i = 0; i < bench_ops; ++i) (void)mb.find(i);
    auto t1 = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::println("Benchmark: {} insert + {} find in {} us = {:.0f} ops/sec", bench_ops, bench_ops, us, 1e6 * bench_ops * 2 / std::max(1.0, static_cast<double>(us)));
    return 0;
}
