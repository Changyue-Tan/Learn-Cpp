/**
 * 18. Optimize a hot loop processing millions of messages (cache/SIMD)
 * Techniques: contiguous data, avoid branches, optional SIMD (e.g. sum array).
 */
#include <vector>
#include <chrono>
#include <numeric>
#include <cstdint>
#include <print>

#ifdef __SSE2__
#include <emmintrin.h>
#endif

// Naive sum
uint64_t sum_naive(const std::vector<uint32_t>& v) {
    uint64_t s = 0;
    for (size_t i = 0; i < v.size(); ++i) s += v[i];
    return s;
}

// Cache-friendly: sequential access, no branch in inner loop
uint64_t sum_cache_friendly(const std::vector<uint32_t>& v) {
    uint64_t s = 0;
    const uint32_t* p = v.data();
    const size_t n = v.size();
    for (size_t i = 0; i < n; ++i)
        s += p[i];
    return s;
}

#ifdef __SSE2__
// SIMD: 4 x 32-bit sums in parallel
uint64_t sum_simd(const std::vector<uint32_t>& v) {
    const size_t n = v.size();
    const uint32_t* p = v.data();
    __m128i sum4 = _mm_setzero_si128();
    size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        __m128i x = _mm_loadu_si128(reinterpret_cast<const __m128i*>(p + i));
        sum4 = _mm_add_epi32(sum4, x);
    }
    alignas(16) int32_t tmp[4];
    _mm_store_si128(reinterpret_cast<__m128i*>(tmp), sum4);
    uint64_t s = tmp[0] + tmp[1] + tmp[2] + tmp[3];
    for (; i < n; ++i) s += p[i];
    return s;
}
#endif

int main() {
    const size_t N = 8 * 1000 * 1000;
    std::vector<uint32_t> v(N);
    std::iota(v.begin(), v.end(), 0);

    auto run = [&v](const char* name, auto fn) {
        auto start = std::chrono::steady_clock::now();
        volatile uint64_t result = fn(v);
        auto end = std::chrono::steady_clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        std::println("{}: {} us (result {})", name, us, static_cast<uint64_t>(result));
    };

    run("naive", sum_naive);
    run("cache-friendly", sum_cache_friendly);
#ifdef __SSE2__
    run("SIMD (SSE2)", sum_simd);
#endif
    double us_min = 1e9;
    auto run_us = [&v](auto fn) {
        auto start = std::chrono::steady_clock::now();
        volatile uint64_t r = fn(v);
        auto end = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    };
    us_min = std::min(us_min, static_cast<double>(run_us(sum_cache_friendly)));
#ifdef __SSE2__
    us_min = std::min(us_min, static_cast<double>(run_us(sum_simd)));
#endif
    double mb_per_sec = (N * sizeof(uint32_t)) / us_min;  // bytes per microsecond ≈ MB/s (1e6 us in 1 s, 1e6 B in 1 MB)
    std::println("Benchmark: {} elements, best ~{:.0f} MB/s ({} us)", N, mb_per_sec, static_cast<long long>(us_min));
    return 0;
}
