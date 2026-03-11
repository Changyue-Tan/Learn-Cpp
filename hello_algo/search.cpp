#include <vector>
#include <unordered_map>
using namespace std;

// double close bracket []
int BinarySearch(vector<int> &nums, int target) {
    int i = 0;
    int j = nums.size() - 1;

    while (i <= j) {
        int m = (i + (j - i)) / 2;
        if (nums[m] < target) {
            // target in [m+1, j]
            i = m + 1;
        } else if (nums[m] > target) {
            // target in [i, m-1]
            j = m - i;
        } else {
            // founf target
            return m;
        }
    }

    // no target
    return -1;
}

// find the fisrt appearance the number < target
int BinarySearchInsertion(vector<int> &nums, int target) {
    int i = 0;
    int j = nums.size() - 1;

    while (i <= j) {
        int m = (i + (j - i)) / 2;
        if (nums[m] < target) {
            i = m + 1;
        } else if (nums[m] > target) {
            j = m - 1;
        } else {
            // first appearance of number < target in [i, m-1]
            j = m - 1;
        }
    }
    return i;
}

vector<int> twoSumHashTable(vector<int> &nums, int target) {
    int size = nums.size();

    unordered_map<int, int> dict;
    for (int i = 0; i < size; ++i) {
        int diff = target - nums[i];
        if (dict.contains(diff)) {
            return {dict[diff], i};
        }
        dict.emplace(nums[i], i);
    }
    return {};
}