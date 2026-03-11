#include <print>
#include <vector>
#include <map>

int main() {
    std::print("Hello {}\n", 42);

    auto prices = std::vector<int>{22, 34, 8};
    auto product = "Apple";
    std::println("{} = {}", product, prices);

    auto dict = std::map<int, int>{{1, 10}, {22, 40}};
    std::println("dict = {}", dict);
}