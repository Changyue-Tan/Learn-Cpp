/**
 * 23. Working-set size vs TLB and huge pages
 *
 * Benchmark that walks memory with page-sized strides for different
 * working-set sizes, to observe the cost when the working set exceeds
 * TLB capacity, and (on Linux) to compare standard pages vs huge pages.
 */

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <unistd.h>

#if defined(__linux__)
#include <sys/mman.h>
#endif

using SteadyClock = std::chrono::steady_clock;

void working_set_benchmark() {
    std::cout << "\n=== Working-set / TLB benchmark ===\n";

    const std::size_t page_size = static_cast<std::size_t>(::getpagesize());

    // Sizes in bytes; tune as desired
    const std::size_t sizes[] = {
        4ULL * 1024,   // 4 KiB
        8ULL * 1024,   // 8 KiB
        16ULL * 1024,   // 16 KiB
        32ULL * 1024,   // 32 KiB
        64ULL * 1024,   // 64 KiB
        128ULL * 1024,   // 128 KiB
        256ULL * 1024,   // 256 KiB
        512ULL * 1024,   // 512 KiB
        1ULL * 1024 * 1024,   // 1 MiB
        4ULL * 1024 * 1024,   // 4 MiB
        16ULL * 1024 * 1024,  // 16 MiB
        32ULL * 1024 * 1024,  // 32 MiB
        64ULL * 1024 * 1024,  // 64 MiB
        128ULL * 1024 * 1024,  // 128 MiB
        256ULL * 1024 * 1024,  // 256 MiB 
        512ULL * 1024 * 1024,  // 512 MiB
        1024ULL * 1024 * 1024 // 1 GiB
        
    };

    const int passes = 8; // multiple sweeps over each working set

    struct Result {
        std::string kind;      // "normal" or "huge"
        std::size_t bytes;     // working-set size in bytes
        double ns_per_touch;   // average cost
    };
    std::vector<Result> results;

    auto run_sweep = [&](std::uint64_t* data, std::size_t bytes,
                         const char* label, const char* kind) {
        const std::size_t elems = bytes / sizeof(std::uint64_t);
        const std::size_t stride = page_size / sizeof(std::uint64_t);
        if (elems == 0 || stride == 0) {
            return;
        }

        volatile std::uint64_t sink = 0;

        auto start = SteadyClock::now();
        for (int p = 0; p < passes; ++p) {
            for (std::size_t i = 0; i < elems; i += stride) {
                data[i] += 1;
                sink += data[i];
            }
        }
        auto end = SteadyClock::now();

        double total_ns =
            std::chrono::duration<double, std::nano>(end - start).count();
        std::size_t touches = (elems / stride + (elems % stride ? 1 : 0)) *
                              static_cast<std::size_t>(passes);
        double ns_per_touch = total_ns / static_cast<double>(touches);

        std::cout << label << " size=" << (bytes / (1024 * 1024)) << " MiB, "
                  << "touches=" << touches << ", "
                  << ns_per_touch << " ns per page-stride access\n";

        results.push_back(Result{kind, bytes, ns_per_touch});
    };

    // Normal pages (malloc / new)
    std::cout << "Standard pages (malloc-backed):\n";
    for (std::size_t bytes : sizes) {
        std::size_t elems = bytes / sizeof(std::uint64_t);
        std::vector<std::uint64_t> buf(elems);
        run_sweep(buf.data(), bytes, "  normal", "normal");
    }

#if defined(__linux__)
    // Huge pages: requires proper system configuration (mount hugetlbfs or THP)
    std::cout << "\nHuge pages (Linux-specific attempt):\n";
    for (std::size_t bytes : sizes) {
        void* ptr = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        if (ptr == MAP_FAILED) {
            std::perror("  mmap hugepage");
            continue;
        }
        run_sweep(static_cast<std::uint64_t*>(ptr), bytes, "  huge", "huge");
        ::munmap(ptr, bytes);
    }
#else
    std::cout << "\nHuge pages: not enabled on this platform build; "
                 "run on Linux with MAP_HUGETLB or THP for huge-page data.\n";
#endif

    // Write results to CSV for external analysis / plotting.
    // File is created in the current working directory.
    const char* csv_name = "23_working_set_results.csv";
    std::ofstream csv(csv_name);
    if (!csv) {
        std::perror("opening CSV file");
        return;
    }
    csv << "kind,size_bytes,size_mib,ns_per_touch\n";
    for (const auto& r : results) {
        double mib = static_cast<double>(r.bytes) / (1024.0 * 1024.0);
        csv << r.kind << ','
            << r.bytes << ','
            << mib << ','
            << r.ns_per_touch << '\n';
    }

    std::cout << "\nWrote CSV results to \"" << csv_name << "\"\n";
}

int main() {
    working_set_benchmark();
    return 0;
}

