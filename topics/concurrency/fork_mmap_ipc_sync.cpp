/**
 * 21. Fork + mmap shared memory IPC with multiple sync methods
 *
 * Parent and child communicate via mmap'd shared memory and synchronise using:
 * 1. Pipe (one pipe for signalling: parent writes, child reads)
 * 2. Two atomics (parent_ready, child_ready flags in shared memory)
 * 3. Process-shared mutex (pthread_mutex in shared region)
 * 4. Process-shared condition variable (pthread_cond + mutex)
 * 5. Process-shared semaphore (unnamed sem_t in shared region)
 *
 * Build: needs -pthread, POSIX. Run: one process forks; parent and child take turns.
 */
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <cstring>
#include <atomic>
#include <chrono>
#include <print>
#include <cstdio>
#include <cerrno>

namespace {

constexpr size_t kSharedSize = 4096;

struct Shared {
    // 1. Two atomics for turn-taking / ready flags
    std::atomic<int> parent_ready{0};
    std::atomic<int> child_ready{0};

    // 2. Process-shared mutex
    pthread_mutex_t mutex;
    // 3. Process-shared condition variable
    pthread_cond_t cond;
    // 4. Process-shared semaphore (we use one)
    sem_t sem;

    std::atomic<int> counter{0};  // atomic for visibility across processes
    char message[64];             // protected by mutex
    bool sem_ok{false};           // true if unnamed sem works (Linux; not on macOS)
};

Shared* g_shared = nullptr;

void init_shared(Shared* s) {
    pthread_mutexattr_t mattr;
    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&s->mutex, &mattr);
    pthread_mutexattr_destroy(&mattr);

    pthread_condattr_t cattr;
    pthread_condattr_init(&cattr);
    pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&s->cond, &cattr);
    pthread_condattr_destroy(&cattr);

#if !defined(__APPLE__)
    s->sem_ok = (sem_init(&s->sem, 1, 0) == 0);
#endif
    s->counter.store(0);
    std::memset(s->message, 0, sizeof(s->message));
}

void destroy_shared(Shared* s) {
#if !defined(__APPLE__)
    if (s->sem_ok) sem_destroy(&s->sem);
#endif
    pthread_cond_destroy(&s->cond);
    pthread_mutex_destroy(&s->mutex);
}

}  // namespace

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);  // unbuffered so forked child output appears
    // Pipe for sync: parent writes, child reads
    int pipe_fds[2];
    if (pipe(pipe_fds) != 0) {
        std::println(stderr, "pipe: {}", strerror(errno));
        return 1;
    }
    const int pipe_read = pipe_fds[0];
    const int pipe_write = pipe_fds[1];

    // Anonymous shared memory (inherited by child after fork)
    void* region = mmap(nullptr, kSharedSize, PROT_READ | PROT_WRITE,
                        MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if (region == MAP_FAILED) {
        std::println(stderr, "mmap: {}", strerror(errno));
        close(pipe_read);
        close(pipe_write);
        return 1;
    }
    g_shared = new (region) Shared();
    init_shared(g_shared);

    pid_t pid = fork();
    if (pid < 0) {
        std::println(stderr, "fork: {}", strerror(errno));
        destroy_shared(g_shared);
        munmap(region, kSharedSize);
        close(pipe_read);
        close(pipe_write);
        return 1;
    }

    if (pid != 0) {
        // ---------- PARENT ----------
        auto parent_t0 = std::chrono::steady_clock::now();
        close(pipe_read);

        // 3. Mutex: set data first so child can read it after pipe/atomics sync
        pthread_mutex_lock(&g_shared->mutex);
        g_shared->counter.store(10, std::memory_order_release);
        snprintf(g_shared->message, sizeof(g_shared->message), "parent says hi");
        pthread_mutex_unlock(&g_shared->mutex);

        // 1. Pipe sync: signal child we're ready
        char byte = 'P';
        write(pipe_write, &byte, 1);
        close(pipe_write);

        // 2. Atomics: parent sets ready, wait for child
        g_shared->parent_ready.store(1, std::memory_order_release);
        while (g_shared->child_ready.load(std::memory_order_acquire) == 0) {
            // spin
        }
        std::println("Parent: atomics sync done (child_ready=1)");

        // 4. Condition variable: signal child (counter -> 20)
        pthread_mutex_lock(&g_shared->mutex);
        g_shared->counter.store(20, std::memory_order_release);
        pthread_cond_signal(&g_shared->cond);
        pthread_mutex_unlock(&g_shared->mutex);

        // 5. Semaphore: post so child can proceed (Linux only; macOS has no unnamed sem in shared memory)
        if (g_shared->sem_ok) sem_post(&g_shared->sem);

        int status = 0;
        waitpid(pid, &status, 0);
        auto parent_t1 = std::chrono::steady_clock::now();
        std::println("Parent: child exited, counter was {}", g_shared->counter.load(std::memory_order_acquire));
        auto ipc_us = std::chrono::duration_cast<std::chrono::microseconds>(parent_t1 - parent_t0).count();
        std::println("Benchmark: 1 IPC round-trip (pipe+atomics+mutex+cond+sem) in {} us", ipc_us);

        destroy_shared(g_shared);
        munmap(region, kSharedSize);
        return 0;
    }

    // ---------- CHILD ----------
    close(pipe_write);

    // 1. Pipe sync: wait for parent
    char byte;
    if (read(pipe_read, &byte, 1) != 1) {
        std::println(stderr, "child: pipe read failed");
        _exit(1);
    }
    close(pipe_read);
    std::println("Child: pipe sync done (got '{}')", byte);

    // 2. Atomics: set child ready
    g_shared->child_ready.store(1, std::memory_order_release);

    // 3. Mutex: read counter and message
    pthread_mutex_lock(&g_shared->mutex);
    int c = g_shared->counter.load(std::memory_order_acquire);
    char msg[64];
    std::memcpy(msg, g_shared->message, sizeof(msg));
    msg[sizeof(msg) - 1] = '\0';  // ensure null-terminated for display
    pthread_mutex_unlock(&g_shared->mutex);
    std::println("Child: mutex read counter={} message='{}'", c, msg);

    // 4. Condition variable: wait for parent signal
    pthread_mutex_lock(&g_shared->mutex);
    while (g_shared->counter.load(std::memory_order_acquire) < 20) {
        pthread_cond_wait(&g_shared->cond, &g_shared->mutex);
    }
    std::println("Child: cond wait done, counter={}", g_shared->counter.load(std::memory_order_acquire));
    pthread_mutex_unlock(&g_shared->mutex);

    // 5. Semaphore: wait for parent post (skip on macOS where unnamed sem isn't supported)
    if (g_shared->sem_ok) {
        sem_wait(&g_shared->sem);
        std::println("Child: sem wait done");
    } else {
        std::println("Child: sem skipped (unnamed sem not available on this OS)");
    }

    g_shared->counter.store(99, std::memory_order_release);  // tell parent we're done
    _exit(0);
}
