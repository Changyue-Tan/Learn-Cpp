#include <vector>
using namespace std;

void selectionSort(vector<int> &nums) {
    int n = nums.size();

    // range to be sorted; [i, n-1]
    // n - 1 to leave an index for j
    for (int i = 0; i < n - 1; ++i) {
        int k = i; 
        // find the smallest element in [i+1, n-1]
        for (int j = i + 1; j < n; ++j) {
            if (nums[j] < nums[k]) {
                // record the index of the smallest element
                k = j;
            }
            // exchange the smallest element with the first element in segment
            swap(nums[i], nums[k]);
        }
    }
}

void bubbleSort(vector<int> &nums) {
    // range to be sorted: [0, i]
    // i > 0 to leave an index for j
    for (int i = nums.size() - 1; i > 0; --i) {
        bool flag = false;
        // bubble the largest element in [0, i] to index i
        for (int j = 0; j < i; ++j) {
            if (nums[j] > nums[j + 1]){
                swap(nums[j], nums[j+1]);
                flag = true;
            }
        }
        if (!flag){
            break; // no swap was made, the vector is sorted
        }
    }
}

void insertionSort(vector<int> &nums) {
    // range already sorted: [0, i-1]
    for (int i = 1; i < nums.size(); ++i) {
        int base = nums[i];
        int j = i - 1;
        // insert base into the correct index in [0, i-1]
        while (j >= 0 && nums[j] > base) {
            nums[j + 1] = nums[j];
            --j;
        }
        nums[j + 1] = base;
    }
}