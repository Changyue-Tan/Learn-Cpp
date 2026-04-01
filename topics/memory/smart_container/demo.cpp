#include <iostream>
#include <string>
#include <algorithm>
#include <numeric>
#include <cassert>

#include "SmartContainer.h"

// Helper class for testing move semantics and construction
class TestObject {
private:
    std::string value_;
    static int construction_count_;
    static int destruction_count_;
    static int copy_count_;
    static int move_count_;

public:
    // Default constructor
    TestObject() : value_("default") { 
        ++construction_count_;
        std::cout << "  [TestObject] Default constructed: " << value_ << "\n";
    }
    
    // Parameterized constructor
    explicit TestObject(const std::string& val) : value_(val) { 
        ++construction_count_;
        std::cout << "  [TestObject] Constructed: " << value_ << "\n";
    }
    
    // Multi-parameter constructor for emplace_back testing
    TestObject(const std::string& prefix, int number) 
        : value_(prefix + std::to_string(number)) { 
        ++construction_count_;
        std::cout << "  [TestObject] Constructed: " << value_ << "\n";
    }
    
    // Copy constructor
    TestObject(const TestObject& other) : value_(other.value_) { 
        ++copy_count_;
        std::cout << "  [TestObject] Copy constructed: " << value_ << "\n";
    }
    
    // Move constructor
    TestObject(TestObject&& other) noexcept : value_(std::move(other.value_)) { 
        ++move_count_;
        std::cout << "  [TestObject] Move constructed: " << value_ << "\n";
        other.value_ = "moved-from";
    }
    
    // Copy assignment
    TestObject& operator=(const TestObject& other) {
        if (this != &other) {
            value_ = other.value_;
            ++copy_count_;
            std::cout << "  [TestObject] Copy assigned: " << value_ << "\n";
        }
        return *this;
    }
    
    // Move assignment
    TestObject& operator=(TestObject&& other) noexcept {
        if (this != &other) {
            value_ = std::move(other.value_);
            ++move_count_;
            std::cout << "  [TestObject] Move assigned: " << value_ << "\n";
            other.value_ = "moved-from";
        }
        return *this;
    }
    
    // Destructor
    ~TestObject() { 
        ++destruction_count_;
        std::cout << "  [TestObject] Destroyed: " << value_ << "\n";
    }
    
    // Accessors
    const std::string& value() const { return value_; }
    void set_value(const std::string& val) { value_ = val; }
    
    // Equality operator
    bool operator==(const TestObject& other) const {
        return value_ == other.value_;
    }
    
    // Less than operator for sorting
    bool operator<(const TestObject& other) const {
        return value_ < other.value_;
    }
    
    // Stream output
    friend std::ostream& operator<<(std::ostream& os, const TestObject& obj) {
        return os << obj.value_;
    }
    
    // Static methods to check object lifecycle
    static void reset_counters() {
        construction_count_ = destruction_count_ = copy_count_ = move_count_ = 0;
    }
    
    static void print_stats() {
        std::cout << "  [Stats] Constructions: " << construction_count_ 
                  << ", Destructions: " << destruction_count_
                  << ", Copies: " << copy_count_ 
                  << ", Moves: " << move_count_ << "\n";
    }
};

// Static member definitions
int TestObject::construction_count_ = 0;
int TestObject::destruction_count_ = 0;
int TestObject::copy_count_ = 0;
int TestObject::move_count_ = 0;

// Utility function to print container contents
template<typename T>
void print_container(const SmartContainer<T>& container, const std::string& label = "") {
    std::cout << "  " << label << "[";
    bool first = true;
    for (const auto& item : container) {
        if (!first) std::cout << ", ";
        std::cout << item;
        first = false;
    }
    std::cout << "] (size: " << container.size() << ")\n";
}

// Function to print section headers
void print_section(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << title << "\n";
    std::cout << std::string(60, '=') << "\n";
}

// Demo functions
void demo_basic_operations() {
    print_section("1. BASIC OPERATIONS DEMO");
    
    std::cout << "Creating empty container...\n";
    SmartContainer<int> container;
    std::cout << "Empty? " << (container.empty() ? "Yes" : "No") << "\n";
    std::cout << "Size: " << container.size() << "\n\n";
    
    std::cout << "Adding elements with push_back...\n";
    for (int i = 1; i <= 5; ++i) {
        container.push_back(i * 10);
        std::cout << "  Added " << i * 10 << ", size now: " << container.size() << "\n";
    }
    print_container(container, "Container: ");
    
    std::cout << "\nTesting element access...\n";
    std::cout << "  Front element: " << container.front() << "\n";
    std::cout << "  Back element: " << container.back() << "\n";
    std::cout << "  Element at index 2: " << container[2] << "\n";
    std::cout << "  Element at index 3 (bounds checked): " << container.at(3) << "\n";
    
    std::cout << "\nModifying elements...\n";
    container[1] = 999;
    container.at(3) = 777;
    print_container(container, "Modified: ");
    
    std::cout << "\nRemoving last element...\n";
    container.pop_back();
    print_container(container, "After pop_back: ");
}

void demo_constructors_and_assignment() {
    print_section("2. CONSTRUCTORS AND ASSIGNMENT DEMO");
    
    std::cout << "Creating container with initializer list...\n";
    SmartContainer<std::string> words{"hello", "world", "from", "smart", "container"};
    print_container(words, "Words: ");
    
    std::cout << "\nCopy constructor...\n";
    SmartContainer<std::string> words_copy(words);
    print_container(words_copy, "Copy: ");
    
    std::cout << "\nMove constructor...\n";
    SmartContainer<std::string> words_moved(std::move(words));
    print_container(words_moved, "Moved: ");
    print_container(words, "Original after move: ");
    
    std::cout << "\nCopy assignment...\n";
    SmartContainer<std::string> words_assigned;
    words_assigned = words_copy;
    print_container(words_assigned, "Copy assigned: ");
    
    std::cout << "\nMove assignment...\n";
    SmartContainer<std::string> words_move_assigned;
    words_move_assigned = std::move(words_copy);
    print_container(words_move_assigned, "Move assigned: ");
    print_container(words_copy, "Original after move assign: ");
}

void demo_advanced_operations() {
    print_section("3. ADVANCED OPERATIONS DEMO");
    
    std::cout << "Testing emplace_back with custom objects...\n";
    TestObject::reset_counters();
    
    SmartContainer<TestObject> objects;
    std::cout << "  Emplacing object with parameters...\n";
    objects.emplace_back("item", 1);
    objects.emplace_back("item", 2);
    
    std::cout << "\n  Adding object via copy...\n";
    TestObject temp("temp_object");
    objects.push_back(temp);
    
    std::cout << "\n  Adding object via move...\n";
    objects.push_back(std::move(temp));
    
    print_container(objects, "Objects: ");
    TestObject::print_stats();
    
    std::cout << "\nClearing container...\n";
    objects.clear();
    std::cout << "Size after clear: " << objects.size() << "\n";
    TestObject::print_stats();
}

void demo_iterators() {
    print_section("4. ITERATOR DEMO");
    
    SmartContainer<int> numbers{3, 1, 4, 1, 5, 9, 2, 6};
    print_container(numbers, "Original: ");
    
    std::cout << "\nUsing range-based for loop to double values...\n";
    for (auto& num : numbers) {
        num *= 2;
    }
    print_container(numbers, "Doubled: ");
    
    std::cout << "\nUsing iterators to find and modify elements...\n";
    auto it = std::find(numbers.begin(), numbers.end(), 8);  // Looking for doubled 4
    if (it != numbers.end()) {
        std::cout << "  Found " << *it << " at position " << (it - numbers.begin()) << "\n";
        *it = 100;
    }
    print_container(numbers, "After modification: ");
    
    std::cout << "\nSorting using standard algorithm...\n";
    std::sort(numbers.begin(), numbers.end());
    print_container(numbers, "Sorted: ");
    
    std::cout << "\nUsing const iterators (read-only)...\n";
    const auto& const_numbers = numbers;
    auto sum = std::accumulate(const_numbers.begin(), const_numbers.end(), 0);
    std::cout << "  Sum of all elements: " << sum << "\n";
    
    std::cout << "\nIterating backwards...\n";
    std::cout << "  Reverse order: [";
    bool first = true;
    for (auto it = numbers.end() - 1; it >= numbers.begin(); --it) {
        if (!first) std::cout << ", ";
        std::cout << *it;
        first = false;
    }
    std::cout << "]\n";
}

void demo_comparison_and_equality() {
    print_section("5. COMPARISON AND EQUALITY DEMO");
    
    SmartContainer<int> container1{1, 2, 3, 4, 5};
    SmartContainer<int> container2{1, 2, 3, 4, 5};
    SmartContainer<int> container3{1, 2, 3, 4, 6};
    SmartContainer<int> container4{1, 2, 3};
    
    print_container(container1, "Container 1: ");
    print_container(container2, "Container 2: ");
    print_container(container3, "Container 3: ");
    print_container(container4, "Container 4: ");
    
    std::cout << "\nEquality comparisons:\n";
    std::cout << "  container1 == container2: " << (container1 == container2 ? "true" : "false") << "\n";
    std::cout << "  container1 == container3: " << (container1 == container3 ? "true" : "false") << "\n";
    std::cout << "  container1 == container4: " << (container1 == container4 ? "true" : "false") << "\n";
}

void demo_exception_safety() {
    print_section("6. EXCEPTION SAFETY DEMO");
    
    SmartContainer<int> container{1, 2, 3};
    print_container(container, "Container: ");
    
    std::cout << "\nTesting bounds checking with at()...\n";
    try {
        std::cout << "  Accessing valid index 1: " << container.at(1) << "\n";
        std::cout << "  Attempting to access invalid index 10...\n";
        container.at(10);  // Should throw
    } catch (const std::out_of_range& e) {
        std::cout << "  Caught exception: " << e.what() << "\n";
    }
    
    std::cout << "\nTesting pop_back on empty container...\n";
    SmartContainer<int> empty_container;
    try {
        empty_container.pop_back();  // Should throw
    } catch (const std::out_of_range& e) {
        std::cout << "  Caught exception: " << e.what() << "\n";
    }
}

void demo_standard_algorithms() {
    print_section("7. STANDARD ALGORITHM COMPATIBILITY DEMO");
    
    SmartContainer<int> numbers{5, 2, 8, 1, 9, 3};
    print_container(numbers, "Original: ");
    
    std::cout << "\nUsing std::find...\n";
    auto found = std::find(numbers.begin(), numbers.end(), 8);
    if (found != numbers.end()) {
        std::cout << "  Found 8 at position " << (found - numbers.begin()) << "\n";
    }
    
    std::cout << "\nUsing std::count_if (counting even numbers)...\n";
    auto even_count = std::count_if(numbers.begin(), numbers.end(), 
                                   [](int n) { return n % 2 == 0; });
    std::cout << "  Even numbers: " << even_count << "\n";
    
    std::cout << "\nUsing std::transform (multiply by 2)...\n";
    std::transform(numbers.begin(), numbers.end(), numbers.begin(),
                   [](int n) { return n * 2; });
    print_container(numbers, "Transformed: ");
    
    std::cout << "\nUsing std::partial_sort...\n";
    std::partial_sort(numbers.begin(), numbers.begin() + 3, numbers.end());
    print_container(numbers, "Partially sorted (first 3): ");
}

void demo_memory_management() {
    print_section("8. MEMORY MANAGEMENT DEMO");
    
    std::cout << "Creating container and monitoring object lifecycle...\n";
    TestObject::reset_counters();
    
    {
        std::cout << "\n--- Entering scope ---\n";
        SmartContainer<TestObject> objects;
        
        std::cout << "\nAdding objects...\n";
        objects.emplace_back("obj1");
        objects.emplace_back("obj2");
        objects.push_back(TestObject("obj3"));
        
        TestObject::print_stats();
        
        std::cout << "\nCopying container...\n";
        SmartContainer<TestObject> objects_copy = objects;
        TestObject::print_stats();
        
        std::cout << "\n--- Exiting scope ---\n";
    }
    
    std::cout << "--- After scope exit ---\n";
    TestObject::print_stats();
    std::cout << "All objects should be properly destroyed!\n";
}

void demo_performance_characteristics() {
    print_section("9. PERFORMANCE CHARACTERISTICS DEMO");
    
    std::cout << "Demonstrating amortized O(1) insertion...\n";
    SmartContainer<int> container;
    
    std::cout << "Adding 20 elements and observing capacity growth:\n";
    for (int i = 1; i <= 20; ++i) {
        container.push_back(i);
        // Note: We don't have a capacity() method in the current implementation,
        // but we can demonstrate the growth pattern by tracking size
        if (i <= 10 || i % 5 == 0) {
            std::cout << "  After " << i << " insertions, size = " << container.size() << "\n";
        }
    }
    
    std::cout << "\nDemonstrating shrink_to_fit...\n";
    std::cout << "  Size before shrink: " << container.size() << "\n";
    container.shrink_to_fit();
    std::cout << "  Size after shrink: " << container.size() << "\n";
    std::cout << "  (Capacity should now equal size)\n";
}

// Main demonstration program
int main() {
    std::cout << "SmartContainer Comprehensive Demo\n";
    std::cout << "=================================\n";
    std::cout << "This demo showcases all features of the SmartContainer class.\n";
    
    try {
        demo_basic_operations();
        demo_constructors_and_assignment();
        demo_advanced_operations();
        demo_iterators();
        demo_comparison_and_equality();
        demo_exception_safety();
        demo_standard_algorithms();
        demo_memory_management();
        demo_performance_characteristics();
        
        print_section("DEMO COMPLETED SUCCESSFULLY!");
        std::cout << "All SmartContainer features demonstrated successfully!\n";
        std::cout << "The container provides:\n";
        std::cout << "  ✓ Dynamic memory management with RAII\n";
        std::cout << "  ✓ Standard container interface\n";
        std::cout << "  ✓ Exception safety\n";
        std::cout << "  ✓ Move semantics support\n";
        std::cout << "  ✓ Iterator compatibility\n";
        std::cout << "  ✓ Standard algorithm support\n";
        std::cout << "  ✓ Proper object lifecycle management\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error during demo: " << e.what() << "\n";
        return 1;
    }
    
    return 0;
}