#include <algorithm>
#include <iostream>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "lib.h"

int main(int argc, char *argv[]) {
	// Parse command line arguments
	std::vector<uint64_t> args;
	for (int i = 1; i < argc; ++i) {
		args.push_back(std::stoull(argv[i]));
	}

	// Parallel computation of factors using std::thread
	std::map<uint64_t, std::vector<uint64_t>> factors;
	std::vector<std::thread> factor_threads;
	std::mutex factors_mutex;

	// Spawn threads for get_factors calls
	for (uint64_t num : args) {
		factor_threads.emplace_back([num, &factors, &factors_mutex]() {
			auto result = get_factors(num);
			std::lock_guard<std::mutex> lock(factors_mutex);
			factors[num] = std::move(result);
		});
	}

	// Wait for all factor computation threads to complete
	for (auto &thread : factor_threads) {
		thread.join();
	}

	// Generate pairs and compute common factors in parallel
	std::vector<std::pair<uint64_t, uint64_t>> factor_pairs;
	for (uint64_t val1 : args) {
		for (uint64_t val2 : args) {
			if (val1 > val2) {
				factor_pairs.emplace_back(val1, val2);
			}
		}
	}

	std::map<std::pair<uint64_t, uint64_t>, std::vector<uint64_t>>
	    common_factors;
	std::vector<std::thread> common_threads;
	std::mutex common_mutex;

	// Spawn threads for get_common_factors calls
	for (const auto &pair : factor_pairs) {
		common_threads.emplace_back(
		    [pair, &factors, &common_factors, &common_mutex]() {
			    auto common = get_common_factors(factors.at(pair.first),
			                                     factors.at(pair.second));
			    std::lock_guard<std::mutex> lock(common_mutex);
			    common_factors[pair] = std::move(common);
		    });
	}

	// Wait for all common factor computation threads to complete
	for (auto &thread : common_threads) {
		thread.join();
	}

	// Sort and print results (same as above)
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
