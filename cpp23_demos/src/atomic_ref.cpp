#include <atomic>

struct OrderBook {
	int best_bid;
};

int main() {
	OrderBook ob;

	std::atomic_ref<int> bid(ob.best_bid);
	bid.store(100, std::memory_order_release);
}
