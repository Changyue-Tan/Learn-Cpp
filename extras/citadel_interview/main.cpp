#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <optional>
#include <queue>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

struct Execution {
    uint64_t id;
    double price;
    uint32_t size;
    uint64_t ts; // arrival time for this feed event
};

// ----------------------------
// Common reconciliation logic
// ----------------------------

static inline bool eq_exec(const Execution &x, const Execution &y) {
    // For speed in benchmarks we use exact compare (use integer ticks in real
    // systems).
    return x.id == y.id && x.size == y.size && x.price == y.price;
}

static inline bool mismatch(const std::optional<Execution> &a,
                            const std::optional<Execution> &b) {
    return a && b && !eq_exec(*a, *b);
}

static inline bool anyMismatch(const std::optional<Execution> &a,
                               const std::optional<Execution> &b,
                               const std::optional<Execution> &c) {
    return mismatch(a, b) || mismatch(a, c) || mismatch(b, c);
}

// -----------------------------------------
// Impl 1: Sorted vector + lower_bound lookup
// Timer expiry: linear scan erase(remove_if)
// -----------------------------------------
class ReconcilerVectorScan {
  public:
    // Window is in the same units as the timestamps you pass in (e.g.
    // milliseconds).
    explicit ReconcilerVectorScan(uint64_t window_width)
        : window_width_(window_width) {}

    void onFeedOne(const Execution &e) { onFeed(Feed::One, e); }
    void onFeedTwo(const Execution &e) { onFeed(Feed::Two, e); }
    void onFeedThree(const Execution &e) { onFeed(Feed::Three, e); }

    // Called periodically. Evicts entries whose window expired.
    void onTimer(uint64_t now) {
        last_timer_ = now;
        by_id_.erase(std::remove_if(by_id_.begin(), by_id_.end(),
                                    [&](const Item &it) {
                                        return isExpired(it.entry, now);
                                    }),
                     by_id_.end());
    }

    uint64_t alerts() const { return alerts_; }

  private:
    enum class Feed { One, Two, Three };

    struct Entry {
        uint64_t first_ts = 0;
        uint64_t last_ts = 0;
        std::optional<Execution> one;
        std::optional<Execution> two;
        std::optional<Execution> three;
        bool alerted =
            false; // avoid spamming repeated alerts for same id within window
    };

    struct Item {
        uint64_t id = 0;
        Entry entry;
    };

    bool isExpired(const Entry &e, uint64_t now) const {
        // Expire based on when we first saw this id.
        return now >= e.first_ts + window_width_;
    }

    Entry &getOrCreate(uint64_t id) {
        auto it = std::lower_bound(
            by_id_.begin(), by_id_.end(), id,
            [](const Item &item, uint64_t key) { return item.id < key; });

        if (it == by_id_.end() || it->id != id) {
            it = by_id_.insert(it, Item{.id = id, .entry = {}});
        }
        return it->entry;
    }

    void onFeed(Feed f, const Execution &exec) {
        const uint64_t ts = exec.ts;
        // Option A: late events whose window already ended are dropped.
        if (ts + window_width_ <= last_timer_)
            return;
        auto &ent = getOrCreate(exec.id);
        if (ent.first_ts == 0 && ent.last_ts == 0) {
            ent.first_ts = ts;
            ent.last_ts = ts;
        } else {
            ent.last_ts = ts;
        }

        switch (f) {
        case Feed::One:
            ent.one = exec;
            break;
        case Feed::Two:
            ent.two = exec;
            break;
        case Feed::Three:
            ent.three = exec;
            break;
        }

        if (!ent.alerted && anyMismatch(ent.one, ent.two, ent.three) &&
            !isExpired(ent, ts)) {
            ent.alerted = true;
            ++alerts_;
        }
    }

    uint64_t window_width_;
    // Sorted by id to allow lower_bound lookup/insert.
    std::vector<Item> by_id_;
    uint64_t alerts_ = 0;
    uint64_t last_timer_ = 0;
};

// -----------------------------------------
// Impl 2: unordered_map
// Timer expiry: linear scan erase
// -----------------------------------------
class ReconcilerUnorderedScan {
  public:
    explicit ReconcilerUnorderedScan(uint64_t window_width)
        : window_width_(window_width) {}

    void onFeedOne(const Execution &e) { onFeed(Feed::One, e); }
    void onFeedTwo(const Execution &e) { onFeed(Feed::Two, e); }
    void onFeedThree(const Execution &e) { onFeed(Feed::Three, e); }

    void onTimer(uint64_t now) {
        last_timer_ = now;
        for (auto it = by_id_.begin(); it != by_id_.end();) {
            if (isExpired(it->second, now))
                it = by_id_.erase(it);
            else
                ++it;
        }
    }

    uint64_t alerts() const { return alerts_; }

  private:
    enum class Feed { One, Two, Three };
    struct Entry {
        uint64_t first_ts = 0;
        uint64_t last_ts = 0;
        std::optional<Execution> one;
        std::optional<Execution> two;
        std::optional<Execution> three;
        bool alerted = false;
    };

    bool isExpired(const Entry &e, uint64_t now) const {
        return now >= e.first_ts + window_width_;
    }

    void onFeed(Feed f, const Execution &exec) {
        const uint64_t ts = exec.ts;
        if (ts + window_width_ <= last_timer_)
            return;
        auto &ent = by_id_[exec.id];
        if (ent.first_ts == 0 && ent.last_ts == 0)
            ent.first_ts = ts;
        ent.last_ts = ts;
        switch (f) {
        case Feed::One:
            ent.one = exec;
            break;
        case Feed::Two:
            ent.two = exec;
            break;
        case Feed::Three:
            ent.three = exec;
            break;
        }
        if (!ent.alerted && anyMismatch(ent.one, ent.two, ent.three) &&
            !isExpired(ent, ts)) {
            ent.alerted = true;
            ++alerts_;
        }
    }

    uint64_t window_width_;
    std::unordered_map<uint64_t, Entry> by_id_;
    uint64_t alerts_ = 0;
    uint64_t last_timer_ = 0;
};

// ---------------------------------------------------------
// Impl 3: unordered_map + min-heap expiry (avoid full scans)
// ---------------------------------------------------------
class ReconcilerUnorderedHeap {
  public:
    explicit ReconcilerUnorderedHeap(uint64_t window_width)
        : window_width_(window_width) {}

    void onFeedOne(const Execution &e) { onFeed(Feed::One, e); }
    void onFeedTwo(const Execution &e) { onFeed(Feed::Two, e); }
    void onFeedThree(const Execution &e) { onFeed(Feed::Three, e); }

    void onTimer(uint64_t now) {
        last_timer_ = now;
        while (!expiry_.empty()) {
            const auto [expire_at, id] = expiry_.top();
            if (expire_at > now)
                break;
            expiry_.pop();
            auto it = by_id_.find(id);
            if (it == by_id_.end())
                continue;
            if (it->second.expire_at == expire_at)
                by_id_.erase(it);
        }
    }

    uint64_t alerts() const { return alerts_; }

  private:
    enum class Feed { One, Two, Three };
    struct Entry {
        uint64_t first_ts = 0;
        uint64_t last_ts = 0;
        uint64_t expire_at = 0;
        std::optional<Execution> one;
        std::optional<Execution> two;
        std::optional<Execution> three;
        bool alerted = false;
    };

    struct Exp {
        uint64_t expire_at;
        uint64_t id;
    };
    struct ExpMin {
        bool operator()(const Exp &a, const Exp &b) const {
            return a.expire_at > b.expire_at;
        }
    };

    void onFeed(Feed f, const Execution &exec) {
        const uint64_t ts = exec.ts;
        if (ts + window_width_ <= last_timer_)
            return;
        auto &ent = by_id_[exec.id];
        if (ent.first_ts == 0 && ent.last_ts == 0)
            ent.first_ts = ts;
        ent.last_ts = ts;
        ent.expire_at = ent.first_ts + window_width_;
        expiry_.push(Exp{ent.expire_at, exec.id});

        switch (f) {
        case Feed::One:
            ent.one = exec;
            break;
        case Feed::Two:
            ent.two = exec;
            break;
        case Feed::Three:
            ent.three = exec;
            break;
        }
        if (!ent.alerted && anyMismatch(ent.one, ent.two, ent.three) &&
            ts < ent.expire_at) {
            ent.alerted = true;
            ++alerts_;
        }
    }

    uint64_t window_width_;
    std::unordered_map<uint64_t, Entry> by_id_;
    std::priority_queue<Exp, std::vector<Exp>, ExpMin> expiry_;
    uint64_t alerts_ = 0;
    uint64_t last_timer_ = 0;
};

// -------------------------------------------------------------------
// Impl 3b: unordered_map (reserved) + min-heap expiry
// Same as Impl 3, but pre-reserves to avoid rehash/allocations.
// -------------------------------------------------------------------
class ReconcilerUnorderedHeapReserved {
  public:
    explicit ReconcilerUnorderedHeapReserved(uint64_t window_width,
                                             size_t expected_ids)
        : window_width_(window_width) {
        by_id_.reserve(expected_ids * 2);
        by_id_.max_load_factor(0.7f);
    }

    void onFeedOne(const Execution &e) { onFeed(Feed::One, e); }
    void onFeedTwo(const Execution &e) { onFeed(Feed::Two, e); }
    void onFeedThree(const Execution &e) { onFeed(Feed::Three, e); }

    void onTimer(uint64_t now) {
        last_timer_ = now;
        while (!expiry_.empty()) {
            const auto [expire_at, id] = expiry_.top();
            if (expire_at > now)
                break;
            expiry_.pop();
            auto it = by_id_.find(id);
            if (it == by_id_.end())
                continue;
            if (it->second.expire_at == expire_at)
                by_id_.erase(it);
        }
    }

    uint64_t alerts() const { return alerts_; }

  private:
    enum class Feed { One, Two, Three };
    struct Entry {
        uint64_t first_ts = 0;
        uint64_t last_ts = 0;
        uint64_t expire_at = 0;
        std::optional<Execution> one;
        std::optional<Execution> two;
        std::optional<Execution> three;
        bool alerted = false;
    };

    struct Exp {
        uint64_t expire_at;
        uint64_t id;
    };
    struct ExpMin {
        bool operator()(const Exp &a, const Exp &b) const {
            return a.expire_at > b.expire_at;
        }
    };

    void onFeed(Feed f, const Execution &exec) {
        const uint64_t ts = exec.ts;
        if (ts + window_width_ <= last_timer_)
            return;
        auto &ent = by_id_[exec.id];
        if (ent.first_ts == 0 && ent.last_ts == 0)
            ent.first_ts = ts;
        ent.last_ts = ts;
        ent.expire_at = ent.first_ts + window_width_;
        expiry_.push(Exp{ent.expire_at, exec.id});

        switch (f) {
        case Feed::One:
            ent.one = exec;
            break;
        case Feed::Two:
            ent.two = exec;
            break;
        case Feed::Three:
            ent.three = exec;
            break;
        }
        if (!ent.alerted && anyMismatch(ent.one, ent.two, ent.three) &&
            ts < ent.expire_at) {
            ent.alerted = true;
            ++alerts_;
        }
    }

    uint64_t window_width_;
    std::unordered_map<uint64_t, Entry> by_id_;
    std::priority_queue<Exp, std::vector<Exp>, ExpMin> expiry_;
    uint64_t alerts_ = 0;
    uint64_t last_timer_ = 0;
};

// ------------------------------------------------------------
// Impl 4: Flat open-addressing hash + min-heap expiry
// (contiguous storage, fewer allocations than unordered_map)
// ------------------------------------------------------------
class ReconcilerFlatHashHeap {
  public:
    explicit ReconcilerFlatHashHeap(uint64_t window_width,
                                    size_t expected_ids = 1 << 20)
        : window_width_(window_width) {
        // power-of-two capacity >= 2*expected (load factor <= 0.5).
        size_t cap = 1;
        while (cap < expected_ids * 2)
            cap <<= 1;
        keys_.assign(cap, kEmpty);
        entries_.resize(cap);
        used_.assign(cap, false);
        mask_ = cap - 1;
    }

    void onFeedOne(const Execution &e) { onFeed(Feed::One, e); }
    void onFeedTwo(const Execution &e) { onFeed(Feed::Two, e); }
    void onFeedThree(const Execution &e) { onFeed(Feed::Three, e); }

    void onTimer(uint64_t now) {
        last_timer_ = now;
        while (!expiry_.empty()) {
            const auto [expire_at, id] = expiry_.top();
            if (expire_at > now)
                break;
            expiry_.pop();
            const size_t idx = findIndex(id);
            if (idx == npos)
                continue;
            if (used_[idx] && entries_[idx].expire_at == expire_at) {
                eraseAt(idx);
            }
        }
    }

    uint64_t alerts() const { return alerts_; }

  private:
    enum class Feed { One, Two, Three };
    struct Entry {
        uint64_t first_ts = 0;
        uint64_t last_ts = 0;
        uint64_t expire_at = 0;
        std::optional<Execution> one;
        std::optional<Execution> two;
        std::optional<Execution> three;
        bool alerted = false;
    };

    struct Exp {
        uint64_t expire_at;
        uint64_t id;
    };
    struct ExpMin {
        bool operator()(const Exp &a, const Exp &b) const {
            return a.expire_at > b.expire_at;
        }
    };

    static constexpr uint64_t kEmpty = 0;
    static constexpr uint64_t kTomb = 1;
    static constexpr size_t npos = static_cast<size_t>(-1);

    static inline uint64_t mix(uint64_t x) {
        // splitmix64
        x += 0x9e3779b97f4a7c15ULL;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
        return x ^ (x >> 31);
    }

    size_t probe(uint64_t id) const {
        return static_cast<size_t>(mix(id)) & mask_;
    }

    size_t findOrInsert(uint64_t id) {
        size_t idx = probe(id);
        size_t first_tomb = npos;
        for (;;) {
            uint64_t k = keys_[idx];
            if (k == kEmpty) {
                if (first_tomb != npos)
                    idx = first_tomb;
                keys_[idx] = id;
                used_[idx] = true;
                entries_[idx] = Entry{};
                return idx;
            }
            if (k == kTomb) {
                if (first_tomb == npos)
                    first_tomb = idx;
            } else if (k == id) {
                return idx;
            }
            idx = (idx + 1) & mask_;
        }
    }

    size_t findIndex(uint64_t id) const {
        size_t idx = probe(id);
        for (;;) {
            uint64_t k = keys_[idx];
            if (k == kEmpty)
                return npos;
            if (k != kTomb && k == id)
                return idx;
            idx = (idx + 1) & mask_;
        }
    }

    void eraseAt(size_t idx) {
        keys_[idx] = kTomb;
        used_[idx] = false;
        entries_[idx] = Entry{};
    }

    void onFeed(Feed f, const Execution &exec) {
        const uint64_t ts = exec.ts;
        if (ts + window_width_ <= last_timer_)
            return;
        const size_t idx = findOrInsert(exec.id);
        auto &ent = entries_[idx];
        if (ent.first_ts == 0 && ent.last_ts == 0)
            ent.first_ts = ts;
        ent.last_ts = ts;
        ent.expire_at = ent.first_ts + window_width_;
        expiry_.push(Exp{ent.expire_at, exec.id});

        switch (f) {
        case Feed::One:
            ent.one = exec;
            break;
        case Feed::Two:
            ent.two = exec;
            break;
        case Feed::Three:
            ent.three = exec;
            break;
        }
        if (!ent.alerted && anyMismatch(ent.one, ent.two, ent.three) &&
            ts < ent.expire_at) {
            ent.alerted = true;
            ++alerts_;
        }
    }

    uint64_t window_width_;
    std::vector<uint64_t> keys_;
    std::vector<Entry> entries_;
    std::vector<uint8_t> used_;
    size_t mask_ = 0;
    std::priority_queue<Exp, std::vector<Exp>, ExpMin> expiry_;
    uint64_t alerts_ = 0;
    uint64_t last_timer_ = 0;
};

// ------------------------------------------------------------
// Impl 4b: Flat open-addressing hash + timer wheel expiry
// Avoids heap operations; best when time is monotonic/tick-like.
// ------------------------------------------------------------
class ReconcilerFlatHashWheel {
  public:
    ReconcilerFlatHashWheel(uint64_t window_width, size_t expected_ids,
                            uint64_t tick_mod)
        : window_width_(window_width), tick_mod_(tick_mod), wheel_(tick_mod) {
        size_t cap = 1;
        while (cap < expected_ids * 2)
            cap <<= 1;
        keys_.assign(cap, kEmpty);
        entries_.resize(cap);
        used_.assign(cap, false);
        mask_ = cap - 1;
    }

    void onFeedOne(const Execution &e) { onFeed(Feed::One, e); }
    void onFeedTwo(const Execution &e) { onFeed(Feed::Two, e); }
    void onFeedThree(const Execution &e) { onFeed(Feed::Three, e); }

    void onTimer(uint64_t now) {
        for (uint64_t t = last_timer_ + 1; t <= now; ++t) {
            auto &bucket = wheel_[t % tick_mod_];
            for (uint64_t id : bucket) {
                const size_t idx = findIndex(id);
                if (idx == npos)
                    continue;
                if (used_[idx] && entries_[idx].expire_at <= t)
                    eraseAt(idx);
            }
            bucket.clear();
        }
        last_timer_ = now;
    }

    uint64_t alerts() const { return alerts_; }

  private:
    enum class Feed { One, Two, Three };
    struct Entry {
        uint64_t first_ts = 0;
        uint64_t last_ts = 0;
        uint64_t expire_at = 0;
        std::optional<Execution> one;
        std::optional<Execution> two;
        std::optional<Execution> three;
        bool alerted = false;
    };

    static constexpr uint64_t kEmpty = 0;
    static constexpr uint64_t kTomb = 1;
    static constexpr size_t npos = static_cast<size_t>(-1);

    static inline uint64_t mix(uint64_t x) {
        x += 0x9e3779b97f4a7c15ULL;
        x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
        x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
        return x ^ (x >> 31);
    }
    size_t probe(uint64_t id) const {
        return static_cast<size_t>(mix(id)) & mask_;
    }

    size_t findOrInsert(uint64_t id) {
        size_t idx = probe(id);
        size_t first_tomb = npos;
        for (;;) {
            uint64_t k = keys_[idx];
            if (k == kEmpty) {
                if (first_tomb != npos)
                    idx = first_tomb;
                keys_[idx] = id;
                used_[idx] = true;
                entries_[idx] = Entry{};
                return idx;
            }
            if (k == kTomb) {
                if (first_tomb == npos)
                    first_tomb = idx;
            } else if (k == id) {
                return idx;
            }
            idx = (idx + 1) & mask_;
        }
    }

    size_t findIndex(uint64_t id) const {
        size_t idx = probe(id);
        for (;;) {
            uint64_t k = keys_[idx];
            if (k == kEmpty)
                return npos;
            if (k != kTomb && k == id)
                return idx;
            idx = (idx + 1) & mask_;
        }
    }

    void eraseAt(size_t idx) {
        keys_[idx] = kTomb;
        used_[idx] = false;
        entries_[idx] = Entry{};
    }

    void schedule(uint64_t id, uint64_t expire_at) {
        wheel_[expire_at % tick_mod_].push_back(id);
    }

    void onFeed(Feed f, const Execution &exec) {
        const uint64_t ts = exec.ts;
        // Option A: late events whose window already ended are dropped.
        if (ts + window_width_ <= last_timer_)
            return;
        const size_t idx = findOrInsert(exec.id);
        auto &ent = entries_[idx];
        if (ent.first_ts == 0 && ent.last_ts == 0) {
            ent.first_ts = ts;
            ent.expire_at = ts + window_width_;
            schedule(exec.id, ent.expire_at);
        }
        ent.last_ts = ts;

        switch (f) {
        case Feed::One:
            ent.one = exec;
            break;
        case Feed::Two:
            ent.two = exec;
            break;
        case Feed::Three:
            ent.three = exec;
            break;
        }
        if (!ent.alerted && anyMismatch(ent.one, ent.two, ent.three) &&
            ts < ent.expire_at) {
            ent.alerted = true;
            ++alerts_;
        }
    }

    uint64_t window_width_;
    uint64_t tick_mod_;
    std::vector<uint64_t> keys_;
    std::vector<Entry> entries_;
    std::vector<uint8_t> used_;
    size_t mask_ = 0;
    std::vector<std::vector<uint64_t>> wheel_;
    uint64_t last_timer_ = 0;
    uint64_t alerts_ = 0;
};

// ------------------------------------------------------------
// Impl 5: Direct-index by id + timer wheel expiry (fastest if
// id space is bounded/dense; onTimer is O(#expired buckets))
// ------------------------------------------------------------
class ReconcilerDirectIndexWheel {
  public:
    ReconcilerDirectIndexWheel(uint64_t window_width, uint64_t max_id,
                               uint64_t tick_mod)
        : window_width_(window_width), max_id_(max_id), tick_mod_(tick_mod),
          entries_(max_id + 1), wheel_(tick_mod) {}

    void onFeedOne(const Execution &e) { onFeed(Feed::One, e); }
    void onFeedTwo(const Execution &e) { onFeed(Feed::Two, e); }
    void onFeedThree(const Execution &e) { onFeed(Feed::Three, e); }

    void onTimer(uint64_t now) {
        // Expire buckets from last_timer_+1..now (mod tick_mod_). Assumes
        // monotonically increasing now.
        for (uint64_t t = last_timer_ + 1; t <= now; ++t) {
            auto &bucket = wheel_[t % tick_mod_];
            for (uint64_t id : bucket) {
                if (id <= max_id_ && entries_[id].active &&
                    entries_[id].expire_at <= t) {
                    entries_[id] = Entry{};
                }
            }
            bucket.clear();
        }
        last_timer_ = now;
    }

    uint64_t alerts() const { return alerts_; }

  private:
    enum class Feed { One, Two, Three };
    struct Entry {
        bool active = false;
        bool alerted = false;
        uint64_t first_ts = 0;
        uint64_t expire_at = 0;
        std::optional<Execution> one;
        std::optional<Execution> two;
        std::optional<Execution> three;
    };

    void scheduleExpiry(uint64_t id, uint64_t expire_at) {
        wheel_[expire_at % tick_mod_].push_back(id);
    }

    void onFeed(Feed f, const Execution &exec) {
        const uint64_t ts = exec.ts;
        // Option A: late events whose window already ended are dropped.
        if (ts + window_width_ <= last_timer_)
            return;
        if (exec.id > max_id_)
            return; // out of configured range
        auto &ent = entries_[exec.id];
        if (!ent.active) {
            ent.active = true;
            ent.first_ts = ts;
            ent.expire_at = ts + window_width_;
            scheduleExpiry(exec.id, ent.expire_at);
        }
        switch (f) {
        case Feed::One:
            ent.one = exec;
            break;
        case Feed::Two:
            ent.two = exec;
            break;
        case Feed::Three:
            ent.three = exec;
            break;
        }
        if (!ent.alerted && anyMismatch(ent.one, ent.two, ent.three) &&
            ts < ent.expire_at) {
            ent.alerted = true;
            ++alerts_;
        }
    }

    uint64_t window_width_;
    uint64_t max_id_;
    uint64_t tick_mod_;
    std::vector<Entry> entries_;
    std::vector<std::vector<uint64_t>> wheel_;
    uint64_t last_timer_ = 0;
    uint64_t alerts_ = 0;
};

// ------------------------------------------------------------
// Impl 5b: Direct-index by id + min-heap expiry
// Useful when you want direct indexing but not timer-wheel buckets.
// ------------------------------------------------------------
class ReconcilerDirectIndexHeap {
  public:
    ReconcilerDirectIndexHeap(uint64_t window_width, uint64_t max_id)
        : window_width_(window_width), max_id_(max_id), entries_(max_id + 1) {}

    void onFeedOne(const Execution &e) { onFeed(Feed::One, e); }
    void onFeedTwo(const Execution &e) { onFeed(Feed::Two, e); }
    void onFeedThree(const Execution &e) { onFeed(Feed::Three, e); }

    void onTimer(uint64_t now) {
        last_timer_ = now;
        while (!expiry_.empty()) {
            const auto top = expiry_.top();
            if (top.expire_at > now)
                break;
            expiry_.pop();
            if (top.id > max_id_)
                continue;
            auto &ent = entries_[top.id];
            if (ent.active && ent.expire_at == top.expire_at)
                ent = Entry{};
        }
    }

    uint64_t alerts() const { return alerts_; }

  private:
    enum class Feed { One, Two, Three };
    struct Entry {
        bool active = false;
        bool alerted = false;
        uint64_t first_ts = 0;
        uint64_t expire_at = 0;
        std::optional<Execution> one;
        std::optional<Execution> two;
        std::optional<Execution> three;
    };
    struct Exp {
        uint64_t expire_at;
        uint64_t id;
    };
    struct ExpMin {
        bool operator()(const Exp &a, const Exp &b) const {
            return a.expire_at > b.expire_at;
        }
    };

    void onFeed(Feed f, const Execution &exec) {
        const uint64_t ts = exec.ts;
        if (ts + window_width_ <= last_timer_)
            return;
        if (exec.id > max_id_)
            return;
        auto &ent = entries_[exec.id];
        if (!ent.active) {
            ent.active = true;
            ent.first_ts = ts;
            ent.expire_at = ts + window_width_;
            expiry_.push(Exp{ent.expire_at, exec.id});
        }
        switch (f) {
        case Feed::One:
            ent.one = exec;
            break;
        case Feed::Two:
            ent.two = exec;
            break;
        case Feed::Three:
            ent.three = exec;
            break;
        }
        if (!ent.alerted && anyMismatch(ent.one, ent.two, ent.three) &&
            ts < ent.expire_at) {
            ent.alerted = true;
            ++alerts_;
        }
    }

    uint64_t window_width_;
    uint64_t max_id_;
    std::vector<Entry> entries_;
    std::priority_queue<Exp, std::vector<Exp>, ExpMin> expiry_;
    uint64_t alerts_ = 0;
    uint64_t last_timer_ = 0;
};

// ----------------------------
// Benchmark harness
// ----------------------------

enum class Feed : uint8_t { One = 1, Two = 2, Three = 3 };

struct Event {
    Feed feed;
    Execution exec;
    uint32_t tie =
        0; // random tie-break when multiple events share the same arrival time
};

static std::vector<Event>
makeWorkload(uint64_t window_width, size_t distinct_ids, size_t events_per_feed,
             double mismatch_rate, uint64_t out_of_order_jitter,
             uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<uint64_t> id_dist(1, distinct_ids);
    std::uniform_real_distribution<double> uni(0.0, 1.0);
    std::uniform_int_distribution<uint64_t> jitter_dist(0, out_of_order_jitter);

    std::vector<Event> events;
    events.reserve(events_per_feed * 3);

    auto baseExec = [&](uint64_t id) -> Execution {
        // Deterministic "base" per id (fast).
        return Execution{id, 100.0 + (id % 1000) * 0.01,
                         static_cast<uint32_t>(1 + (id % 10)), 0};
    };

    uint64_t t = 1;
    for (size_t n = 0; n < events_per_feed; ++n) {
        uint64_t id = id_dist(rng);
        Execution b = baseExec(id);

        Execution e1 = b;
        Execution e2 = b;
        Execution e3 = b;

        if (uni(rng) < mismatch_rate) {
            // Introduce mismatch in one random feed.
            int which = static_cast<int>(rng() % 3);
            if (which == 0)
                e1.price += 0.01;
            else if (which == 1)
                e2.size += 1;
            else
                e3.price -= 0.02;
        }

        const uint64_t t1 = t + jitter_dist(rng);
        const uint64_t t2 = t + jitter_dist(rng);
        const uint64_t t3 = t + jitter_dist(rng);

        e1.ts = t1;
        e2.ts = t2;
        e3.ts = t3;
        events.push_back(Event{Feed::One, e1, static_cast<uint32_t>(rng())});
        events.push_back(Event{Feed::Two, e2, static_cast<uint32_t>(rng())});
        events.push_back(Event{Feed::Three, e3, static_cast<uint32_t>(rng())});

        // Advance "logical time" so windows are meaningful.
        t += 1 + (window_width / 20);
    }

    // Arrival times must be non-decreasing for Option A + tumbling windows:
    // `Execution.ts` is the arrival time. Sort by ts; random `tie` shuffles
    // order among equal timestamps.
    std::sort(events.begin(), events.end(), [](const Event &a, const Event &b) {
        if (a.exec.ts != b.exec.ts)
            return a.exec.ts < b.exec.ts;
        return a.tie < b.tie;
    });
    return events;
}

template <typename R>
static uint64_t runBench(const std::vector<Event> &events, R &r,
                         uint64_t window_width) {
    uint64_t now = 0;
    // Fire timer at each *window end* (tumbling windows): W, 2W, 3W, ...
    uint64_t next_timer = window_width;
    for (const auto &ev : events) {
        const uint64_t ts = ev.exec.ts;
        if (ts > now)
            now = ts;
        while (now >= next_timer) {
            r.onTimer(next_timer);
            next_timer += window_width;
        }
        switch (ev.feed) {
        case Feed::One:
            r.onFeedOne(ev.exec);
            break;
        case Feed::Two:
            r.onFeedTwo(ev.exec);
            break;
        case Feed::Three:
            r.onFeedThree(ev.exec);
            break;
        }
    }
    // Flush: advance a few windows to ensure all pending items expire.
    const uint64_t flush_to = ((now / window_width) + 3) * window_width;
    while (next_timer <= flush_to) {
        r.onTimer(next_timer);
        next_timer += window_width;
    }
    return r.alerts();
}

template <typename MakeReconciler>
static void benchOne(const std::string &name, const std::vector<Event> &events,
                     uint64_t window_width, MakeReconciler make) {
    auto r = make();
    // Warmup
    runBench(events, r, window_width);

    auto r2 = make();
    const auto t0 = std::chrono::steady_clock::now();
    const uint64_t alerts = runBench(events, r2, window_width);
    const auto t1 = std::chrono::steady_clock::now();
    const auto ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    const double ns_per_event =
        static_cast<double>(ns) / static_cast<double>(events.size());

    std::cout << std::left << std::setw(28) << name << "  " << std::right
              << std::setw(10) << std::fixed << std::setprecision(2)
              << ns_per_event << " ns/event" << "  alerts=" << alerts << "\n";
}

int main() {
    const uint64_t window_width = 1'000;
    const size_t distinct_ids = 200'000;
    const size_t events_per_feed = 200'000;
    const double mismatch_rate = 0.02;
    const uint64_t out_of_order_jitter = 200;

    const auto events =
        makeWorkload(window_width, distinct_ids, events_per_feed, mismatch_rate,
                     out_of_order_jitter,
                     /*seed=*/1234567);

    std::cout << "events=" << events.size() << " distinct_ids~" << distinct_ids
              << " window=" << window_width
              << " mismatch_rate=" << mismatch_rate << "\n\n";

    benchOne("VectorScan", events, window_width,
             [&] { return ReconcilerVectorScan(window_width); });
    benchOne("UnorderedScan", events, window_width, [&] {
        ReconcilerUnorderedScan r(window_width);
        return r;
    });
    benchOne("UnorderedHeap", events, window_width, [&] {
        ReconcilerUnorderedHeap r(window_width);
        return r;
    });
    benchOne("UnorderedHeapReserved", events, window_width, [&] {
        return ReconcilerUnorderedHeapReserved(window_width, distinct_ids);
    });
    benchOne("FlatHashHeap", events, window_width, [&] {
        return ReconcilerFlatHashHeap(window_width, distinct_ids);
    });
    benchOne("FlatHashWheel", events, window_width, [&] {
        const uint64_t tick_mod = 4096;
        return ReconcilerFlatHashWheel(window_width, distinct_ids, tick_mod);
    });
    benchOne("DirectIndexWheel", events, window_width, [&] {
        // Configure wheel modulo as a power-of-two-ish bucket count.
        // Here we use 2*window to reduce collisions; must exceed max expire_at
        // granularity in benchmark.
        const uint64_t tick_mod = 4096;
        return ReconcilerDirectIndexWheel(window_width, distinct_ids + 5,
                                          tick_mod);
    });
    benchOne("DirectIndexHeap", events, window_width, [&] {
        return ReconcilerDirectIndexHeap(window_width, distinct_ids + 5);
    });

    return 0;
}