/**
 * 14. TCP echo server using nonblocking sockets
 * accept/send/recv with O_NONBLOCK; simple single-thread loop (no epoll for minimal demo).
 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <vector>
#include <chrono>
#include <errno.h>
#include <print>
#include <cstdio>

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { std::println(stderr, "socket failed"); return 1; }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8888);
    if (bind(listen_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::println(stderr, "bind failed"); close(listen_fd); return 1;
    }
    if (listen(listen_fd, 128) < 0) {
        std::println(stderr, "listen failed"); close(listen_fd); return 1;
    }
    set_nonblocking(listen_fd);

    std::println("Echo server on :8888 (nonblocking). Connect and send text.");

    std::vector<int> clients;
    char buf[1024];
    const auto bench_start = std::chrono::steady_clock::now();
    constexpr auto bench_duration = std::chrono::milliseconds(100);
    uint64_t loop_iters = 0;

    while (true) {
        int fd = accept(listen_fd, nullptr, nullptr);
        if (fd >= 0) {
            set_nonblocking(fd);
            clients.push_back(fd);
            std::println("client connected, fd={}", fd);
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            break;
        }

        for (size_t i = 0; i < clients.size(); ) {
            ssize_t n = recv(clients[i], buf, sizeof(buf), 0);
            if (n > 0) {
                send(clients[i], buf, static_cast<size_t>(n), 0);
            } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                close(clients[i]);
                clients[i] = clients.back();
                clients.pop_back();
                continue;
            }
            ++i;
        }
        ++loop_iters;
        if (clients.empty() && std::chrono::steady_clock::now() - bench_start >= bench_duration) {
            std::println("Benchmark: {} empty loop iters in {} ms = {:.0f} iter/sec",
                         loop_iters, bench_duration.count(), 1000.0 * loop_iters / bench_duration.count());
            break;
        }
    }
    for (int c : clients) close(c);
    close(listen_fd);
    return 0;
}
