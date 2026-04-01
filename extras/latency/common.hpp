/**
 * Shared types and utilities for low-latency system variants.
 */
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <queue>

#define CACHE_LINE_SIZE 64
#define CACHE_ALIGNED alignas(CACHE_LINE_SIZE)

#if defined(__x86_64__)
#define CPU_PAUSE() __asm__ volatile("pause" ::: "memory")
#else
#define CPU_PAUSE() std::atomic_signal_fence(std::memory_order_seq_cst)
#endif

struct alignas(64) Message {
  int64_t recv_ns;
  uint32_t seq;
  uint16_t len;
  char data[46];
};

static_assert(sizeof(Message) == 64, "Message must be exactly one cache line");

inline int64_t now_ns() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (int64_t)ts.tv_sec * 1'000'000'000LL + ts.tv_nsec;
}

// ─── Memory pool (optional: use in pool variants) ───
template <typename T, size_t Capacity> class MemoryPool {
  static_assert(sizeof(T) >= sizeof(void *), "Block too small for free-list");

public:
  MemoryPool() {
    for (size_t i = 0; i < Capacity - 1; ++i)
      *reinterpret_cast<Block **>(&storage_[i]) = &storage_[i + 1];
    *reinterpret_cast<Block **>(&storage_[Capacity - 1]) = nullptr;
    free_head_ = &storage_[0];
  }
  T *alloc() noexcept {
    if (!free_head_)
      return nullptr;
    Block *blk = free_head_;
    free_head_ = *reinterpret_cast<Block **>(blk);
    return reinterpret_cast<T *>(blk);
  }
  void free(T *ptr) noexcept {
    Block *blk = reinterpret_cast<Block *>(ptr);
    *reinterpret_cast<Block **>(blk) = free_head_;
    free_head_ = blk;
  }
  size_t capacity() const { return Capacity; }

private:
  struct alignas(alignof(T)) Block {
    char raw[sizeof(T)];
  };
  Block storage_[Capacity];
  Block *free_head_;
};

// ─── Lock-free SPSC queue ───
template <typename T, size_t N> class SPSCQueue {
  static_assert((N & (N - 1)) == 0, "N must be power of 2");

public:
  bool push(T *item) noexcept {
    size_t w = write_idx_.load(std::memory_order_relaxed);
    size_t next_w = (w + 1) & mask_;
    if (next_w == read_idx_.load(std::memory_order_acquire))
      return false;
    slots_[w] = item;
    write_idx_.store(next_w, std::memory_order_release);
    return true;
  }
  T *pop() noexcept {
    size_t r = read_idx_.load(std::memory_order_relaxed);
    if (r == write_idx_.load(std::memory_order_acquire))
      return nullptr;
    T *item = slots_[r];
    read_idx_.store((r + 1) & mask_, std::memory_order_release);
    return item;
  }

private:
  static constexpr size_t mask_ = N - 1;
  CACHE_ALIGNED std::atomic<size_t> write_idx_{0};
  CACHE_ALIGNED std::atomic<size_t> read_idx_{0};
  T *slots_[N];
};

// ─── Mutex-based queue (for locking variant) ───
template <typename T> class MutexQueue {
public:
  bool push(T *item) {
    std::lock_guard<std::mutex> lk(mu_);
    if (q_.size() >= MaxSize)
      return false;
    q_.push(item);
    return true;
  }
  T *pop() {
    std::lock_guard<std::mutex> lk(mu_);
    if (q_.empty())
      return nullptr;
    T *item = q_.front();
    q_.pop();
    return item;
  }

private:
  static constexpr size_t MaxSize = 512;
  std::mutex mu_;
  std::queue<T *> q_;
};
