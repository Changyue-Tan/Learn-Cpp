// concepts_demo.cpp
#include <concepts>
#include <print>

template <typename T>
concept Integral = std::integral<T>;

template <Integral T>
T add(T a, T b) {
    return a + b;
}

int main() {
    std::println("2 + 3 = {}", add(2, 3));
    // add(3.5, 2.1); // compile error
}