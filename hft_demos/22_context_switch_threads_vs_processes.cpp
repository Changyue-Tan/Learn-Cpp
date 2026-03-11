/**
 * 22. Context switch overhead: threads vs processes
 *
 * Simple ping-pong benchmark to estimate the cost of context switches
 * between two threads in a single process vs two separate processes
 * communicating via pipes.
 */

#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstring>
#include <iostream>
#include <mutex>
#include <thread>

#include <sys/wait.h>
#include <unistd.h>

using SteadyClock = std::chrono::steady_clock;

static const int ITERS = 1'000'000;

// ---------- Thread ping-pong ----------

void thread_ping_pong() {
    std::mutex m;
    std::condition_variable cv;
    bool turn = false; // false: thread A, true: thread B
    bool done = false;

    auto worker = [&](bool my_turn) {
        std::unique_lock<std::mutex> lock(m);
        for (int i = 0; i < ITERS; ++i) {
            cv.wait(lock, [&] { return turn == my_turn || done; });
            if (done) break;
            turn = !turn;
            cv.notify_one();
        }
    };

    std::thread tB(worker, true);

    // Ensure initial state
    {
        std::unique_lock<std::mutex> lock(m);
        turn = false;
    }

    auto start = SteadyClock::now();
    {
        std::unique_lock<std::mutex> lock(m);
        for (int i = 0; i < ITERS; ++i) {
            cv.wait(lock, [&] { return turn == false; });
            turn = true;        // hand off to B
            cv.notify_one();
        }
        done = true;
        cv.notify_one();
    }
    auto end = SteadyClock::now();

    tB.join();

    double total_ns =
        std::chrono::duration<double, std::nano>(end - start).count();
    double switches = 2.0 * ITERS; // A->B and B->A per iteration

    std::cout << "Threads: total " << (total_ns / 1e6) << " ms, "
              << (total_ns / switches) << " ns per switch\n";
}

// ---------- Process ping-pong ----------

void process_ping_pong() {
    int p2c[2]; // parent -> child
    int c2p[2]; // child -> parent
    if (pipe(p2c) == -1 || pipe(c2p) == -1) {
        std::perror("pipe");
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        std::perror("fork");
        return;
    }

    if (pid == 0) {
        // Child
        close(p2c[1]); // close parent write
        close(c2p[0]); // close parent read

        char buf = 0;
        for (int i = 0; i < ITERS; ++i) {
            if (read(p2c[0], &buf, 1) != 1) {
                std::perror("read child");
                _exit(1);
            }
            if (write(c2p[1], &buf, 1) != 1) {
                std::perror("write child");
                _exit(1);
            }
        }
        _exit(0);
    } else {
        // Parent
        close(p2c[0]); // close child read
        close(c2p[1]); // close child write

        char buf = 0;

        auto start = SteadyClock::now();
        for (int i = 0; i < ITERS; ++i) {
            if (write(p2c[1], &buf, 1) != 1) {
                std::perror("write parent");
                break;
            }
            if (read(c2p[0], &buf, 1) != 1) {
                std::perror("read parent");
                break;
            }
        }
        auto end = SteadyClock::now();

        close(p2c[1]);
        close(c2p[0]);
        int status = 0;
        waitpid(pid, &status, 0);

        double total_ns =
            std::chrono::duration<double, std::nano>(end - start).count();
        double switches = 2.0 * ITERS; // parent->child and child->parent per iteration

        std::cout << "Processes: total " << (total_ns / 1e6) << " ms, "
                  << (total_ns / switches) << " ns per switch\n";
    }
}

int main() {
    std::cout << "ITERS = " << ITERS << "\n";
    thread_ping_pong();
    process_ping_pong();
    return 0;
}

