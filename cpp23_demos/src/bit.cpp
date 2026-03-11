#include <bit>
#include <print>

auto main() -> int {
	auto x = 0x12345678;
	auto y = std::byteswap(x);
	std::println("original: 0x{:x}", x);
	std::println("swapped : 0x{:x}", y);

	auto f = 1.23f;
	auto raw = std::bit_cast<uint32_t>(f);
    std::println("original : {}", f);
	std::println("bitcasted: {}", raw);
}
