/**
 * 15. Event loop using epoll (Linux) / kqueue (macOS, BSD)
 * Single-threaded: register fds, wait for events, dispatch callbacks.
 */
#include <vector>
#include <functional>
#include <map>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <chrono>
#include <print>
#include <stdexcept>

#if defined(__linux__)
#include <sys/epoll.h>
#define USE_EPOLL 1
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <sys/event.h>
#include <sys/types.h>
#define USE_KQUEUE 1
#else
#error "epoll or kqueue required"
#endif

class EventLoop {
public:
    EventLoop() {
#if USE_EPOLL
        epoll_fd_ = epoll_create1(0);
        if (epoll_fd_ < 0) throw std::runtime_error("epoll_create1");
#else
        kq_ = kqueue();
        if (kq_ < 0) throw std::runtime_error("kqueue");
#endif
    }

    ~EventLoop() {
#if USE_EPOLL
        if (epoll_fd_ >= 0) close(epoll_fd_);
#else
        if (kq_ >= 0) close(kq_);
#endif
    }

    using Callback = std::function<void()>;

    void add_fd(int fd, Callback on_read) {
        callbacks_[fd] = std::move(on_read);
#if USE_EPOLL
        epoll_event ev{};
        ev.events = EPOLLIN;
        ev.data.fd = fd;
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ev) < 0)
            throw std::runtime_error("epoll_ctl add");
#else
        struct kevent kev;
        EV_SET(&kev, fd, EVFILT_READ, EV_ADD, 0, 0, nullptr);
        if (kevent(kq_, &kev, 1, nullptr, 0, nullptr) < 0)
            throw std::runtime_error("kevent add");
#endif
    }

    void run_once(int timeout_ms) {
#if USE_EPOLL
        epoll_event events[64];
        int n = epoll_wait(epoll_fd_, events, 64, timeout_ms);
        if (n < 0) return;
        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            auto it = callbacks_.find(fd);
            if (it != callbacks_.end()) it->second();
        }
#else
        struct kevent evs[64];
        struct timespec ts = { timeout_ms / 1000, (timeout_ms % 1000) * 1000000 };
        int n = kevent(kq_, nullptr, 0, evs, 64, timeout_ms >= 0 ? &ts : nullptr);
        if (n < 0) return;
        for (int i = 0; i < n; ++i) {
            int fd = (int)evs[i].ident;
            auto it = callbacks_.find(fd);
            if (it != callbacks_.end()) it->second();
        }
#endif
    }

private:
#if USE_EPOLL
    int epoll_fd_ = -1;
#else
    int kq_ = -1;
#endif
    std::map<int, Callback> callbacks_;
};

int main() {
    EventLoop loop;
    int pipe_fds[2];
    if (pipe(pipe_fds) < 0) { std::println(stderr, "pipe failed"); return 1; }
    char buf[64];
    loop.add_fd(pipe_fds[0], [&]() {
        ssize_t n = read(pipe_fds[0], buf, sizeof(buf));
        if (n > 0) std::println("read {} bytes from pipe", n);
    });
    write(pipe_fds[1], "hello", 5);
    loop.run_once(100);
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    std::println("Event loop (epoll/kqueue) demo done.");

    // Benchmark: run_once (no fds) throughput
    EventLoop loopb;
    const int bench_iters = 200'000;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < bench_iters; ++i) loopb.run_once(0);
    auto t1 = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::println("Benchmark: {} run_once(0) in {} us = {:.0f} ops/sec", bench_iters, us, 1e6 * bench_iters / std::max(1.0, static_cast<double>(us)));
    return 0;
}
