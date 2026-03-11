#include <thread>
#include <functional>
#include <print>

void increment(int& x) {
    x++;
}

int main() {
    int value = 10;

    std::thread t(increment, std::ref(value));
    t.join();

    std::println("{}", value);
}