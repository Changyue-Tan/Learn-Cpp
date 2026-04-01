#include <atomic>
#include <chrono>
#include <print>
#include <span>
#include <thread>
#include <vector>

template <typename T> class RingBuffer {
  public:
	explicit RingBuffer(size_t capacity)
	    : buffer_(capacity), capacity_(capacity), head_(0), tail_(0) {}

	bool push(const T &value) {
		auto head = head_.load(std::memory_order_relaxed);
		auto next = (head + 1) % capacity_;
		if (next == tail_.load(std::memory_order_acquire))
			return false;  // full
		buffer_[head] = value;
		head_.store(next, std::memory_order_release);
		return true;
	}

	bool pop(T &value) {
		auto tail = tail_.load(std::memory_order_relaxed);
		if (tail == head_.load(std::memory_order_acquire))
			return false;  // empty
		value = buffer_[tail];
		tail_.store((tail + 1) % capacity_, std::memory_order_release);
		return true;
	}

	std::span<T> raw_buffer() {
		return {buffer_};
	}

	std::pair<size_t, size_t> indices() const {
		return {tail_.load(std::memory_order_acquire),
		        head_.load(std::memory_order_acquire)};
	}

  private:
	std::vector<T> buffer_;
	size_t capacity_;
	std::atomic<size_t> head_;
	std::atomic<size_t> tail_;
};

int main() {
	RingBuffer<int> rb(10);

	// Producer
	std::jthread producer([&rb](std::stop_token st) {
		int i = 0;
		while (!st.stop_requested()) {
			if (rb.push(i)) {
				std::println("Produced: {}", i);
				++i;
			} else {
                std::println("Queue full");
            }
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}
	});

	// Consumer
	std::jthread consumer([&rb](std::stop_token st) {
		int value;
		while (!st.stop_requested()) {
			if (rb.pop(value)) {
				std::println("Consumed: {}", value);
			} else {
                std::println("Nothing to consume");
            }
			std::this_thread::sleep_for(std::chrono::milliseconds(150));
		}
	});

	// Monitor: print raw buffer + visual pointers
	std::jthread monitor([&rb](std::stop_token st) {
		while (!st.stop_requested()) {
			// Print raw buffer
			auto buf = rb.raw_buffer();
			std::println("Buffer: {}", buf);

			// Print head and tail indices
			auto [tail, head] = rb.indices();
			std::println("Tail: {}, Head: {}", tail, head);

			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
	});

	std::this_thread::sleep_for(std::chrono::seconds(5));
	std::println("Stopping threads...");
}