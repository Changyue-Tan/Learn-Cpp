// ranges_demo.cpp
#include <ranges>
#include <vector>
#include <print>

int main() {
    std::vector<int> v{1,2,3,4,5,6, 7, 8};

    auto result =
        v
        | std::views::filter([](int x){ return x % 2 == 0; })
        | std::views::transform([](int x){ return x * x; });

    std::println("{}", result);
}