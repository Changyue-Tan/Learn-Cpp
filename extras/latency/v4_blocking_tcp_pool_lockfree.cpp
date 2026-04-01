/**
 * Variant 4: blocking TCP (one thread per accept, blocking recv) + memory pool
 * + lock-free SPSC. Build: g++ -std=c++20 -O2 -o v4_blocking_tcp_pool_lockfree
 * v4_blocking_tcp_pool_lockfree.cpp -lpthread
 */
#include "common.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <iostream>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

constexpr size_t POOL_SIZE = 1024;
constexpr size_t QUEUE_SIZE = 512;
constexpr uint16_t PORT = 9999;

static MemoryPool<Message, POOL_SIZE> g_pool;
// Lock-free SPSC queue is still used between per-connection threads and the
// single consumer, but network I/O is now blocking and threaded instead of
// event-driven.
static SPSCQueue<Message, QUEUE_SIZE> g_queue;

// Global termination flag and latency aggregates shared by all threads.
static std::atomic<bool> g_running{true};
static std::atomic<int64_t> g_total_ns{0};
static std::atomic<uint64_t> g_count{0};
static std::atomic<int64_t> g_max_ns{0};
static std::atomic<uint32_t> g_seq{0};
// Listen socket; main closes it after g_running=false so accept() unblocks and
// the network thread can exit (otherwise it would block in accept() forever).
static std::atomic<int> g_listen_fd{-1};

static void set_tcp_nodelay(int fd) {
  int yes = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
}

void consumer_thread() {
  // Single consumer thread drains the queue fed by all client_handler threads
  // and performs the same latency accounting as in the epoll-based variants.
  while (g_running.load(std::memory_order_relaxed)) {
    Message *msg = g_queue.pop();
    if (!msg) {
      CPU_PAUSE();
      continue;
    }
    int64_t latency = now_ns() - msg->recv_ns;
    g_total_ns.fetch_add(latency, std::memory_order_relaxed);
    g_count.fetch_add(1, std::memory_order_relaxed);
    int64_t prev = g_max_ns.load(std::memory_order_relaxed);
    while (latency > prev && !g_max_ns.compare_exchange_weak(
                                 prev, latency, std::memory_order_relaxed)) {
    }
    if (msg->len >= 4) {
      uint32_t price;
      std::memcpy(&price, msg->data, sizeof(price));
      (void)price;
    }
    g_pool.free(msg);
  }
}

void client_handler(int conn) {
  set_tcp_nodelay(conn);
  char buf[1024];
  ssize_t bytes;
  while (g_running.load(std::memory_order_relaxed) &&
         (bytes = recv(conn, buf, sizeof(buf), 0)) > 0) {
    int64_t ts = now_ns();
    Message *msg = g_pool.alloc();
    if (!msg)
      continue;
    uint32_t seq = g_seq.fetch_add(1, std::memory_order_relaxed);
    msg->recv_ns = ts;
    msg->seq = seq;
    msg->len =
        static_cast<uint16_t>(std::min((ssize_t)sizeof(msg->data), bytes));
    std::memcpy(msg->data, buf, msg->len);
    while (!g_queue.push(msg))
      CPU_PAUSE();
  }
  close(conn);
}

void network_thread() {
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(PORT);
  if (bind(listen_fd, (sockaddr *)&addr, sizeof(addr)) < 0)
    throw std::runtime_error("bind failed");
  if (listen(listen_fd, 10) < 0)
    throw std::runtime_error("listen failed");
  g_listen_fd.store(listen_fd, std::memory_order_release);
  while (g_running.load(std::memory_order_relaxed)) {
    sockaddr_in peer{};
    socklen_t plen = sizeof(peer);
    int conn = accept(listen_fd, (sockaddr *)&peer, &plen);
    if (conn < 0)
      continue;
    std::thread(client_handler, conn).detach();
  }
  // listen_fd may already be closed by main(); do not close again.
}

void stats_thread() {
  while (g_running.load(std::memory_order_relaxed)) {
    std::this_thread::sleep_for(std::chrono::seconds(2));
    uint64_t cnt = g_count.load(std::memory_order_relaxed);
    if (cnt > 0)
      std::cout << "[STATS] count=" << cnt << " avg_ns="
                << (g_total_ns.load(std::memory_order_relaxed) / (int64_t)cnt)
                << " max_ns=" << g_max_ns.load(std::memory_order_relaxed)
                << "\n";
  }
}

int main() {
  std::cout << "VARIANT=blocking_tcp_pool_lockfree\n";
  std::thread t_net(network_thread), t_con(consumer_thread),
      t_sta(stats_thread);
  std::cin.get();
  g_running.store(false, std::memory_order_relaxed);
  int fd = g_listen_fd.exchange(-1, std::memory_order_acq_rel);
  if (fd >= 0)
    close(fd);
  t_net.join();
  t_con.join();
  t_sta.join();
  uint64_t cnt = g_count.load();
  if (cnt > 0)
    std::cout << "FINAL_STATS count=" << cnt
              << " avg_ns=" << (g_total_ns.load() / (int64_t)cnt)
              << " max_ns=" << g_max_ns.load() << "\n";
  return 0;
}
