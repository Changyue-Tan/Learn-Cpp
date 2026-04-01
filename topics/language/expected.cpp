// expected_chain_demo.cpp
#include <expected>
#include <print>
#include <string>

// Step 1: divide
std::expected<int, std::string> divide(int a, int b) {
	if (b == 0)
		return std::unexpected("division by zero");
	return a / b;
}

// Step 2: multiply (also returns expected)
std::expected<int, std::string> multiply(int x, int factor) {
	return x * factor;
}

// Step 3: validate result
std::expected<int, std::string> validate_positive(int x) {
	if (x < 0)
		return std::unexpected("result is negative");
	return x;
}

int main() {
	auto result =
	    divide(20, 2)
	        .and_then([](int x) { return multiply(x, 3); })
	        .and_then(validate_positive)
	        .transform([](int x) { return x + 1; })
	        .and_then([](int x) -> std::expected<void, std::string> {
		        std::println("final value = {}", x);
		        return {};
	        })
	        .or_else(
	            [](const std::string &err) -> std::expected<void, std::string> {
		            std::println("error = {}", err);
		            return {};
	            });

	// Failure path demo
	auto err =
	    divide(10, 0)
	        .and_then([](int x) { return multiply(x, 3); })
	        .or_else([](const std::string &err) {
		        std::println("failure path: {}", err);
		        return std::expected<int, std::string>{std::unexpected(err)};
	        });
}