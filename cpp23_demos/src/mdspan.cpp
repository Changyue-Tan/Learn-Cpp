// mdspan_demo.cpp
#include <mdspan>
#include <vector>
#include <print>

int main() {
    std::vector<double> storage(6);

    std::mdspan m(storage.data(), 2, 3);

    for (size_t i = 0; i < 2; ++i)
        for (size_t j = 0; j < 3; ++j)
            m[i, j] = i * 10 + j;

    std::println("{}", storage);
}