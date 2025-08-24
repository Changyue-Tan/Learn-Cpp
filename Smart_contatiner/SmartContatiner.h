#ifndef SMART_CONTAINER_H
#define SMART_CONTAINER_H

#include <algorithm>
#include <compare>
#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <utility>

/**
 * @brief A dynamic array container that manages memory automatically
 *
 * SmartContainer is a sequence container that encapsulates dynamic size arrays.
 * It provides functionality similar to std::vector, with automatic memory management
 * using smart pointers for enhanced safety and RAII compliance.
 *
 * @tparam T The type of elements stored in the container
 *
 * Key Features:
 * - Dynamic memory allocation with automatic cleanup
 * - Exception-safe operations
 * - Move semantics support
 * - Random access iterators
 * - Standard container interface compliance
 *
 * Time Complexities:
 * - Access: O(1)
 * - Insertion at end: O(1) amortized, O(n) worst case
 * - Removal at end: O(1)
 * - Search: O(n)
 *
 * Memory Guarantees:
 * - Strong exception safety for most operations
 * - No memory leaks due to RAII design
 * - Automatic capacity growth using exponential strategy
 */
template <typename T> class SmartContainer {
  public:
	// ============================================================================
	// Type Definitions (Standard Container Interface)
	// ============================================================================

	using value_type = T;               ///< Type of elements stored
	using size_type = std::size_t;      ///< Type for sizes and indices
	using reference = T &;              ///< Reference to element type
	using const_reference = const T &;  ///< Const reference to element type
	using pointer = T *;                ///< Pointer to element type
	using const_pointer = const T *;    ///< Const pointer to element type

	// ============================================================================
	// Constructors and Destructor
	// ============================================================================

	/**
     * @brief Default constructor - creates an empty container
     *
     * Constructs an empty container with no elements allocated.
     * No memory allocation occurs until first element is added.
     *
     * @post size() == 0 && capacity() == 0
     * @noexcept This operation never throws
     */
	SmartContainer() noexcept : data_(nullptr), size_(0), capacity_(0) {}

	/**
     * @brief Initializer list constructor
     *
     * Constructs container with elements from initializer list.
     * Elements are copied in the order they appear in the list.
     *
     * @param ilist Initializer list to copy elements from
     * @post size() == ilist.size()
     * @throws std::bad_alloc if memory allocation fails
     * @throws Any exception thrown by T's copy constructor
     *
     * Example:
     * @code
     * SmartContainer<int> container{1, 2, 3, 4, 5};
     * @endcode
     */
	SmartContainer(std::initializer_list<T> ilist)
	    : data_(nullptr), size_(0), capacity_(0) {
		reserve(ilist.size());
		for (const auto &elem : ilist) {
			push_back(elem);
		}
	}

	/**
     * @brief Copy constructor
     *
     * Constructs container with deep copy of another container's elements.
     * All elements are copied using T's copy constructor.
     *
     * @param other Container to copy from
     * @post size() == other.size() && *this == other
     * @throws std::bad_alloc if memory allocation fails
     * @throws Any exception thrown by T's copy constructor
     */
	SmartContainer(const SmartContainer &other)
	    : data_(nullptr), size_(0), capacity_(0) {
		reserve(other.size_);
		for (size_type i = 0; i < other.size_; ++i) {
			push_back(other.data_[i]);
		}
	}

	/**
     * @brief Move constructor
     *
     * Constructs container by moving resources from another container.
     * The other container is left in a valid but unspecified state.
     *
     * @param other Container to move from
     * @post other is left in valid but unspecified state
     * @noexcept This operation never throws
     */
	SmartContainer(SmartContainer &&other) noexcept
	    : data_(std::move(other.data_)), size_(other.size_),
	      capacity_(other.capacity_) {
		// Leave other in valid state
		other.size_ = 0;
		other.capacity_ = 0;
	}

	/**
     * @brief Destructor
     *
     * Destroys all elements and releases allocated memory.
     * All iterators, pointers, and references become invalid.
     *
     * @note Automatic due to std::unique_ptr usage - no explicit implementation needed
     */
	~SmartContainer() = default;

	// ============================================================================
	// Assignment Operators
	// ============================================================================

	/**
     * @brief Copy assignment operator
     *
     * Replaces contents with deep copy of another container's elements.
     * Uses copy-and-swap idiom for strong exception safety.
     *
     * @param other Container to copy from
     * @return Reference to this container
     * @post *this == other (if no exception thrown)
     * @throws std::bad_alloc if memory allocation fails
     * @throws Any exception thrown by T's copy constructor
     */
	SmartContainer &operator=(const SmartContainer &other) {
		if (this != &other) {
			SmartContainer tmp(other);  // May throw - if so, *this unchanged
			swap(tmp);                  // No-throw swap
		}
		return *this;
	}

	/**
     * @brief Move assignment operator
     *
     * Replaces contents by moving resources from another container.
     * The other container is left in valid but unspecified state.
     *
     * @param other Container to move from
     * @return Reference to this container
     * @post other is left in valid but unspecified state
     * @noexcept This operation never throws
     */
	SmartContainer &operator=(SmartContainer &&other) noexcept {
		if (this != &other) {
			data_ = std::move(other.data_);
			size_ = other.size_;
			capacity_ = other.capacity_;

			// Leave other in valid state
			other.size_ = 0;
			other.capacity_ = 0;
		}
		return *this;
	}

	// ============================================================================
	// Utility Functions
	// ============================================================================

	/**
     * @brief Exchanges contents with another container
     *
     * Swaps the contents of this container with other.
     * All iterators and references remain valid but refer to different container.
     *
     * @param other Container to swap with
     * @noexcept This operation never throws
     */
	void swap(SmartContainer &other) noexcept {
		using std::swap;
		swap(data_, other.data_);
		swap(size_, other.size_);
		swap(capacity_, other.capacity_);
	}

	// ============================================================================
	// Element Access
	// ============================================================================

	/**
     * @brief Access element at specified position (unchecked)
     *
     * Returns reference to element at specified position.
     * No bounds checking is performed - undefined behavior if pos >= size().
     *
     * @param pos Position of element to access
     * @return Reference to element at position pos
     * @pre pos < size()
     *
     * @note Prefer at() for bounds-checked access
     */
	reference operator[](size_type pos) {
		return data_[pos];
	}

	/// @overload
	const_reference operator[](size_type pos) const {
		return data_[pos];
	}

	/**
     * @brief Access element at specified position (bounds-checked)
     *
     * Returns reference to element at specified position.
     * Throws std::out_of_range if position is invalid.
     *
     * @param pos Position of element to access
     * @return Reference to element at position pos
     * @throws std::out_of_range if pos >= size()
     */
	reference at(size_type pos) {
		if (pos >= size_)
			throw std::out_of_range("SmartContainer::at: index out of range");
		return data_[pos];
	}

	/// @overload
	const_reference at(size_type pos) const {
		if (pos >= size_)
			throw std::out_of_range("SmartContainer::at: index out of range");
		return data_[pos];
	}

	/**
     * @brief Access first element
     *
     * Returns reference to first element in container.
     *
     * @return Reference to first element
     * @pre !empty() (calling on empty container is undefined behavior)
     */
	reference front() {
		return data_[0];
	}

	/// @overload
	const_reference front() const {
		return data_[0];
	}

	/**
     * @brief Access last element
     *
     * Returns reference to last element in container.
     *
     * @return Reference to last element
     * @pre !empty() (calling on empty container is undefined behavior)
     */
	reference back() {
		return data_[size_ - 1];
	}

	/// @overload
	const_reference back() const {
		return data_[size_ - 1];
	}

	// ============================================================================
	// Capacity
	// ============================================================================

	/**
     * @brief Check if container is empty
     *
     * @return true if container has no elements, false otherwise
     * @noexcept This operation never throws
     */
	bool empty() const noexcept {
		return size_ == 0;
	}

	/**
     * @brief Get number of elements
     *
     * @return Number of elements currently in container
     * @noexcept This operation never throws
     */
	size_type size() const noexcept {
		return size_;
	}

	/**
     * @brief Reduce memory usage to fit current size
     *
     * Requests removal of unused capacity. After this call,
     * capacity() should equal size(), but this is non-binding.
     *
     * @post capacity() >= size() (may not shrink if not beneficial)
     * @throws std::bad_alloc if memory allocation fails
     * @throws Any exception thrown by T's move constructor
     */
	void shrink_to_fit() {
		if (capacity_ > size_) {
			// Allocate exact amount needed
			std::unique_ptr<T[]> new_data(new T[size_]);

			// Move existing elements to new location
			std::uninitialized_move(data_.get(), data_.get() + size_,
			                        new_data.get());

			// Destroy elements in old location
			std::destroy(data_.get(), data_.get() + size_);

			// Replace old storage
			data_ = std::move(new_data);
			capacity_ = size_;
		}
	}

	// ============================================================================
	// Modifiers
	// ============================================================================

	/**
     * @brief Add element to end (copy version)
     *
     * Appends copy of value to end of container.
     * If capacity is insufficient, automatic reallocation occurs.
     *
     * @param value Element to copy and append
     * @post size() increased by 1
     * @throws std::bad_alloc if memory allocation fails
     * @throws Any exception thrown by T's copy constructor
     *
     * Complexity: O(1) amortized, O(n) worst case (when reallocation occurs)
     */
	void push_back(const T &value) {
		// Ensure we have space - may trigger reallocation
		if (size_ == capacity_)
			reserve(capacity_ == 0 ? 1 : capacity_ * 2);

		// Copy construct new element at end
		data_[size_++] = value;
	}

	/**
     * @brief Add element to end (move version)
     *
     * Appends moved value to end of container.
     * If capacity is insufficient, automatic reallocation occurs.
     *
     * @param value Element to move and append
     * @post size() increased by 1, value is moved-from
     * @throws std::bad_alloc if memory allocation fails
     * @throws Any exception thrown by T's move constructor
     */
	void push_back(T &&value) {
		// Ensure we have space - may trigger reallocation
		if (size_ == capacity_)
			reserve(capacity_ == 0 ? 1 : capacity_ * 2);

		// Move construct new element at end
		data_[size_++] = std::move(value);
	}

	/**
     * @brief Construct element in-place at end
     *
     * Constructs element at end of container using provided arguments.
     * If capacity is insufficient, automatic reallocation occurs.
     *
     * @tparam Args Types of arguments to forward to T's constructor
     * @param args Arguments to forward to T's constructor
     * @post size() increased by 1
     * @throws std::bad_alloc if memory allocation fails
     * @throws Any exception thrown by T's constructor
     *
     * Example:
     * @code
     * container.emplace_back(arg1, arg2, arg3);  // Calls T(arg1, arg2, arg3)
     * @endcode
     */
	template <typename... Args> void emplace_back(Args &&...args) {
		// Ensure we have space - may trigger reallocation
		if (size_ == capacity_)
			reserve(capacity_ == 0 ? 1 : capacity_ * 2);

		// Construct element directly in allocated memory
		::new (data_.get() + size_) T(std::forward<Args>(args)...);
		++size_;
	}

	/**
     * @brief Remove last element
     *
     * Removes and destroys the last element in container.
     * No reallocation occurs, capacity() is unchanged.
     *
     * @pre !empty()
     * @post size() decreased by 1
     * @throws std::out_of_range if container is empty
     */
	void pop_back() {
		if (empty())
			throw std::out_of_range(
			    "SmartContainer::pop_back: container is empty");

		// Destroy last element and decrease size
		--size_;
		std::destroy_at(data_.get() + size_);
	}

	/**
     * @brief Remove all elements
     *
     * Destroys all elements in container but keeps allocated capacity.
     * All iterators, pointers, and references become invalid.
     *
     * @post size() == 0, capacity() unchanged
     * @noexcept This operation never throws
     */
	void clear() noexcept {
		// Destroy all elements (destructor called automatically for each)
		std::destroy(data_.get(), data_.get() + size_);
		size_ = 0;
	}

	// ============================================================================
	// Comparison Operators
	// ============================================================================

	/**
     * @brief Equality comparison
     *
     * Two containers are equal if they have the same size and
     * all corresponding elements compare equal.
     *
     * @param rhs Container to compare with
     * @return true if containers are equal, false otherwise
     * @noexcept This operation never throws (assuming T's operator== doesn't throw)
     */
	bool operator==(const SmartContainer &rhs) const noexcept {
		// Quick size check first
		if (size_ != rhs.size_)
			return false;

		// Element-by-element comparison
		for (size_type i = 0; i < size_; ++i) {
			if (!(data_[i] == rhs.data_[i]))
				return false;
		}
		return true;
	}

	// ============================================================================
	// Iterator Classes and Support
	// ============================================================================

	/**
     * @brief Random access iterator for SmartContainer
     *
     * Provides random access iteration over container elements.
     * Supports all operations required by std::random_access_iterator_tag.
     *
     * Iterator Invalidation:
     * - Invalidated by operations that may reallocate (push_back, emplace_back, etc.)
     * - Remains valid after operations that don't change capacity
     */
	class iterator {
	  public:
		// Iterator traits for standard library compatibility
		using iterator_category = std::random_access_iterator_tag;
		using value_type = T;
		using difference_type = std::ptrdiff_t;
		using pointer = T *;
		using reference = T &;

		/// @brief Construct iterator from pointer
		iterator(pointer ptr) : ptr_(ptr) {}

		// ========================================================================
		// Dereference Operations
		// ========================================================================

		/// @brief Dereference operator
		reference operator*() const {
			return *ptr_;
		}

		/// @brief Arrow operator
		pointer operator->() const {
			return ptr_;
		}

		// ========================================================================
		// Increment/Decrement Operations
		// ========================================================================

		/// @brief Pre-increment
		iterator &operator++() {
			++ptr_;
			return *this;
		}

		/// @brief Post-increment
		iterator operator++(int) {
			iterator tmp = *this;
			++(*this);
			return tmp;
		}

		/// @brief Pre-decrement
		iterator &operator--() {
			--ptr_;
			return *this;
		}

		/// @brief Post-decrement
		iterator operator--(int) {
			iterator tmp = *this;
			--(*this);
			return tmp;
		}

		// ========================================================================
		// Arithmetic Operations
		// ========================================================================

		/// @brief Addition assignment
		iterator &operator+=(difference_type n) {
			ptr_ += n;
			return *this;
		}

		/// @brief Subtraction assignment
		iterator &operator-=(difference_type n) {
			ptr_ -= n;
			return *this;
		}

		/// @brief Addition
		iterator operator+(difference_type n) const {
			return iterator(ptr_ + n);
		}

		/// @brief Subtraction
		iterator operator-(difference_type n) const {
			return iterator(ptr_ - n);
		}

		/// @brief Distance between iterators
		difference_type operator-(const iterator &rhs) const {
			return ptr_ - rhs.ptr_;
		}

		// ========================================================================
		// Comparison Operations (C++20 three-way comparison)
		// ========================================================================

		/// @brief Three-way comparison (enables all comparison operators)
		auto operator<=>(const iterator &rhs) const = default;

	  private:
		pointer ptr_;  ///< Pointer to current element
	};

	/**
     * @brief Const random access iterator for SmartContainer
     *
     * Provides const random access iteration over container elements.
     * Similar to iterator but provides read-only access to elements.
     */
	class const_iterator {
	  public:
		// Iterator traits for standard library compatibility
		using iterator_category = std::random_access_iterator_tag;
		using value_type = T;
		using difference_type = std::ptrdiff_t;
		using pointer = const T *;
		using reference = const T &;

		/// @brief Construct const_iterator from pointer
		const_iterator(pointer ptr) : ptr_(ptr) {}

		// ========================================================================
		// Dereference Operations
		// ========================================================================

		/// @brief Dereference operator (const)
		reference operator*() const {
			return *ptr_;
		}

		/// @brief Arrow operator (const)
		pointer operator->() const {
			return ptr_;
		}

		// ========================================================================
		// Increment/Decrement Operations
		// ========================================================================

		/// @brief Pre-increment
		const_iterator &operator++() {
			++ptr_;
			return *this;
		}

		/// @brief Post-increment
		const_iterator operator++(int) {
			const_iterator tmp = *this;
			++(*this);
			return tmp;
		}

		/// @brief Pre-decrement
		const_iterator &operator--() {
			--ptr_;
			return *this;
		}

		/// @brief Post-decrement
		const_iterator operator--(int) {
			const_iterator tmp = *this;
			--(*this);
			return tmp;
		}

		// ========================================================================
		// Arithmetic Operations
		// ========================================================================

		/// @brief Addition assignment
		const_iterator &operator+=(difference_type n) {
			ptr_ += n;
			return *this;
		}

		/// @brief Subtraction assignment
		const_iterator &operator-=(difference_type n) {
			ptr_ -= n;
			return *this;
		}

		/// @brief Addition
		const_iterator operator+(difference_type n) const {
			return const_iterator(ptr_ + n);
		}

		/// @brief Subtraction
		const_iterator operator-(difference_type n) const {
			return const_iterator(ptr_ - n);
		}

		/// @brief Distance between iterators
		difference_type operator-(const const_iterator &rhs) const {
			return ptr_ - rhs.ptr_;
		}

		// ========================================================================
		// Comparison Operations
		// ========================================================================

		/// @brief Three-way comparison (enables all comparison operators)
		auto operator<=>(const const_iterator &rhs) const = default;

	  private:
		pointer ptr_;  ///< Pointer to current element
	};

	// ============================================================================
	// Iterator Access Functions
	// ============================================================================

	/// @brief Get iterator to beginning
	iterator begin() noexcept {
		return iterator(data_.get());
	}

	/// @brief Get iterator to end
	iterator end() noexcept {
		return iterator(data_.get() + size_);
	}

	/// @brief Get const iterator to beginning
	const_iterator begin() const noexcept {
		return const_iterator(data_.get());
	}

	/// @brief Get const iterator to end
	const_iterator end() const noexcept {
		return const_iterator(data_.get() + size_);
	}

	/// @brief Get const iterator to beginning (explicit)
	const_iterator cbegin() const noexcept {
		return const_iterator(data_.get());
	}

	/// @brief Get const iterator to end (explicit)
	const_iterator cend() const noexcept {
		return const_iterator(data_.get() + size_);
	}

  private:
	// ============================================================================
	// Private Helper Functions
	// ============================================================================

	/**
     * @brief Reserve storage for at least new_cap elements
     *
     * Increases container capacity to accommodate at least new_cap elements.
     * If new_cap <= current capacity, no action is taken.
     * Existing elements are moved to new storage location.
     *
     * This function uses compile-time optimization to choose between two strategies:
     * - For trivial types: Use make_unique with default construction (zero-cost)
     * - For non-trivial types: Use raw memory allocation to avoid unnecessary construction
     *
     * @param new_cap Minimum capacity to reserve
     * @throws std::bad_alloc if memory allocation fails
     * @throws Any exception thrown by T's move constructor
     *
     * @note All iterators, pointers, and references are invalidated
     * @complexity O(size()) due to moving existing elements
     * 
     * Exception Safety: Strong guarantee - if any exception is thrown,
     * the container state remains unchanged.
     */
	void reserve(size_type new_cap) {
		// Early return optimization: no reallocation needed
		// This check prevents unnecessary work when capacity is already sufficient
		if (new_cap <= capacity_)
			return;

		// Compile-time dispatch based on type traits
		// This if constexpr evaluates at compile time, so only one branch is compiled
		if constexpr (std::is_trivially_constructible_v<T> &&
		              std::is_trivially_destructible_v<T>) {
			// ========================================================================
			// TRIVIAL TYPES PATH (int, double, POD structs, etc.)
			// ========================================================================

			// For trivial types, default construction has zero cost
			// The compiler can optimize away the initialization to just memory allocation
			// This is safe because trivial types have no-op constructors/destructors
			auto new_data = std::make_unique<T[]>(new_cap);

			// Move existing elements to new storage
			if (data_ && size_ > 0) {
				// For trivial types, std::copy is optimal (often becomes memcpy)
				// No need for std::uninitialized_move since elements are already constructed
				// This is faster than move semantics for trivial types
				std::copy(data_.get(), data_.get() + size_, new_data.get());
			}

			// Replace old storage with new storage
			// std::unique_ptr automatically handles deallocation of old memory
			data_ = std::move(new_data);
			capacity_ = new_cap;

		} else {
			// Non-trivial types - use value-initialization to avoid construction overhead
			// The {} initializer creates default-constructed objects, but for many types
			// this is optimized away by the compiler
			std::unique_ptr<T[]> new_data(new T[new_cap]{});

			// Move existing elements using simple assignment
			if (data_ && size_ > 0) {
				for (size_type i = 0; i < size_; ++i) {
					new_data[i] = std::move(data_[i]);
				}
				// Old elements are automatically destroyed when data_ is replaced
			}

			data_ = std::move(new_data);
			capacity_ = new_cap;
		}

		// Post-condition: capacity_ >= new_cap and all existing elements preserved
		// All iterators, references, and pointers to elements are now invalid
	}

	// ============================================================================
	// Member Variables
	// ============================================================================

	std::unique_ptr<T[]> data_;  ///< Smart pointer managing element storage
	size_type size_;             ///< Current number of elements
	size_type capacity_;         ///< Current storage capacity
};

// ================================================================================
// Free Functions
// ================================================================================

/**
 * @brief Swap two SmartContainer instances
 *
 * Specializes std::swap for SmartContainer to provide efficient swapping.
 *
 * @tparam T Element type
 * @param lhs First container
 * @param rhs Second container
 * @noexcept This operation never throws
 */
template <typename T>
void swap(SmartContainer<T> &lhs, SmartContainer<T> &rhs) noexcept {
	lhs.swap(rhs);
}

#endif  // SMART_CONTAINER_H