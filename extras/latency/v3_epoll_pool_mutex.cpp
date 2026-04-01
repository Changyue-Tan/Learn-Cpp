/**
 * Variant 3: epoll + memory pool + mutex-based queue (locking).
 * Build: g++ -std=c++20 -O2 -o v3_epoll_pool_mutex v3_epoll_pool_mutex.cpp
 * -lpthread
 */
#include "common.hpp"
#include <arpa/inet.h>
#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

constexpr size_t POOL_SIZE = 1024;
constexpr uint16_t PORT = 9999;

static MemoryPool<Message, POOL_SIZE> g_pool;
// Mutex-protected queue contrasts with the lock-free SPSC queue in v1/v2.
static MutexQueue<Message> g_queue;

// Shared termination flag and latency statistics, same structure as v1/v2.
static std::atomic<bool> g_running{true};
static std::atomic<int64_t> g_total_ns{0};
static std::atomic<uint64_t> g_count{0};
static std::atomic<int64_t> g_max_ns{0};

static void set_nonblocking(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
static void set_tcp_nodelay(int fd) {
  int yes = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
}

void consumer_thread() {
  // Measure the impact of locking in the inter-thread handoff path while
  // keeping the rest of the pipeline (epoll, memory pool, stats) identical.
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

void network_thread() {
  int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  int opt = 1;
  setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
  set_nonblocking(listen_fd);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(PORT);
  if (bind(listen_fd, (sockaddr *)&addr, sizeof(addr)) < 0)
    throw std::runtime_error("bind failed");
  if (listen(listen_fd, 10) < 0)
    throw std::runtime_error("listen failed");
  int ep = epoll_create1(0);
  epoll_event ev{};
  ev.events = EPOLLIN | EPOLLET;
  ev.data.fd = listen_fd;
  epoll_ctl(ep, EPOLL_CTL_ADD, listen_fd, &ev);
  epoll_event events[32];
  uint32_t seq = 0;
  while (g_running.load(std::memory_order_relaxed)) {
    int n = epoll_wait(ep, events, 32, 100);
    for (int i = 0; i < n; ++i) {
      int fd = events[i].data.fd;
      if (fd == listen_fd) {
        sockaddr_in peer{};
        socklen_t plen = sizeof(peer);
        int conn = accept4(listen_fd, (sockaddr *)&peer, &plen, SOCK_NONBLOCK);
        if (conn >= 0) {
          set_tcp_nodelay(conn);
          epoll_event cev{};
          cev.events = EPOLLIN | EPOLLET;
          cev.data.fd = conn;
          epoll_ctl(ep, EPOLL_CTL_ADD, conn, &cev);
        }
        continue;
      }
      char buf[1024];
      ssize_t bytes;
      while ((bytes = recv(fd, buf, sizeof(buf), 0)) > 0) {
        int64_t ts = now_ns();
        Message *msg = g_pool.alloc();
        if (!msg)
          continue;
        msg->recv_ns = ts;
        msg->seq = seq++;
        msg->len =
            static_cast<uint16_t>(std::min((ssize_t)sizeof(msg->data), bytes));
        std::memcpy(msg->data, buf, msg->len);
        while (!g_queue.push(msg))
          CPU_PAUSE();
      }
      if (bytes == 0) {
        epoll_ctl(ep, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);
      }
    }
  }
  close(listen_fd);
  close(ep);
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
  std::cout << "VARIANT=epoll_pool_mutex\n";
  std::thread t_net(network_thread), t_con(consumer_thread),
      t_sta(stats_thread);
  std::cin.get();
  g_running.store(false, std::memory_order_relaxed);
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
