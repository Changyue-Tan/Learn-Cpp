#include <future>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

#include "lib.h"

int main(int argc, char *argv[]) {
	// Parse command line arguments
	std::vector<uint64_t> args;
	for (int i = 1; i < argc; ++i) {
		args.push_back(std::stoull(argv[i]));
	}

	// Parallel computation of factors for each number
	std::map<uint64_t, std::vector<uint64_t>> factors;
	std::vector<std::future<std::pair<uint64_t, std::vector<uint64_t>>>>
	    factor_futures;

	// Spawn threads for get_factors calls
	for (uint64_t num : args) {
		factor_futures.push_back(std::async(std::launch::async, [num]() {
			return std::make_pair(num, get_factors(num));
		}));
	}

	// Collect results from factor computation threads
	for (auto &future : factor_futures) {
		auto result = future.get();
		factors[result.first] = std::move(result.second);
	}

	// Generate pairs where val1 > val2
	std::vector<std::pair<uint64_t, uint64_t>> factor_pairs;
	for (uint64_t val1 : args) {
		for (uint64_t val2 : args) {
			if (val1 > val2) {
				factor_pairs.emplace_back(val1, val2);
			}
		}
	}

	// Parallel computation of common factors
	std::map<std::pair<uint64_t, uint64_t>, std::vector<uint64_t>>
	    common_factors;
	std::vector<std::future<
	    std::pair<std::pair<uint64_t, uint64_t>, std::vector<uint64_t>>>>
	    common_futures;

	// Spawn threads for get_common_factors calls
	for (const auto &pair : factor_pairs) {
		uint64_t a = pair.first;
		uint64_t b = pair.second;

		common_futures.push_back(
		    std::async(std::launch::async, [a, b, &factors]() {
			    auto common = get_common_factors(factors.at(a), factors.at(b));
			    return std::make_pair(std::make_pair(a, b), std::move(common));
		    }));
	}

	// Collect results from common factors computation
	for (auto &future : common_futures) {
		auto result = future.get();
		common_factors[result.first] = std::move(result.second);
	}

	// Sort keys and print results
	std::vector<std::pair<uint64_t, uint64_t>> keys;
	for (const auto &entry : common_factors) {
		keys.push_back(entry.first);
	}
	std::sort(keys.begin(), keys.end());

	for (const auto &key : keys) {
		std::cout << "(" << key.first << ", " << key.second << "): [";
		const auto &factors_list = common_factors[key];
		for (size_t i = 0; i < factors_list.size(); ++i) {
			if (i > 0)
				std::cout << ", ";
			std::cout << factors_list[i];
		}
		std::cout << "]" << std::endl;
	}

	return 0;
}
