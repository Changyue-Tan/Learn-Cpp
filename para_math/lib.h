#include <algorithm>
#include <thread>
#include <vector>

// Library functions (equivalent to lib.rs)
std::vector<uint64_t> get_factors(uint64_t num) {
	std::vector<uint64_t> factors;
	for (uint64_t i = 2; i <= num; ++i) {
		while (num % i == 0) {
			factors.push_back(i);
			num = num / i;
		}
		if (num == 1) {
			break;
		}
	}
	return factors;
}

std::vector<uint64_t> get_common_factors(const std::vector<uint64_t> &list1,
                                         const std::vector<uint64_t> &list2) {
	std::vector<uint64_t> list2_copy = list2;  // Make a copy to modify
	std::vector<uint64_t> factors;

	for (uint64_t elem : list1) {
		auto it = std::find(list2_copy.begin(), list2_copy.end(), elem);
		if (it != list2_copy.end()) {
			factors.push_back(elem);
			list2_copy.erase(it);  // Remove found element
		}
	}

	return factors;
}