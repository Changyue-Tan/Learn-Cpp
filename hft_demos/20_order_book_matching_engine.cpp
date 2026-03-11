/**
 * 20. Simple order book / matching engine
 * Price-time priority: limit orders at best bid/ask; match incoming against book.
 */
#include <map>
#include <vector>
#include <cstdint>
#include <chrono>
#include <print>

using OrderId = uint64_t;
using Price = int64_t;   // in ticks or cents
using Quantity = int64_t;

enum class Side { Buy, Sell };

struct Order {
    OrderId id;
    Side side;
    Price price;
    Quantity qty;
    Order(OrderId i, Side s, Price p, Quantity q) : id(i), side(s), price(p), qty(q) {}
};

struct Fill {
    OrderId order_id;
    OrderId match_id;
    Price price;
    Quantity qty;
};

class OrderBook {
public:
    std::vector<Fill> add_order(Order order) {
        std::vector<Fill> fills;
        if (order.side == Side::Buy)
            match_buy(order, fills);
        else
            match_sell(order, fills);
        if (order.qty > 0) {
            if (order.side == Side::Buy)
                bids_[order.price].push_back(order);
            else
                asks_[order.price].push_back(order);
        }
        return fills;
    }

    Price best_bid() const {
        return bids_.empty() ? 0 : bids_.rbegin()->first;
    }
    Price best_ask() const {
        return asks_.empty() ? 0 : asks_.begin()->first;
    }

private:
    void match_buy(Order& order, std::vector<Fill>& fills) {
        while (order.qty > 0 && !asks_.empty() && asks_.begin()->first <= order.price) {
            auto& level = asks_.begin()->second;
            while (order.qty > 0 && !level.empty()) {
                Order& resting = level.front();
                Quantity fill_qty = std::min(order.qty, resting.qty);
                fills.push_back({order.id, resting.id, resting.price, fill_qty});
                order.qty -= fill_qty;
                resting.qty -= fill_qty;
                if (resting.qty == 0) level.erase(level.begin());
            }
            if (level.empty()) asks_.erase(asks_.begin());
        }
    }

    void match_sell(Order& order, std::vector<Fill>& fills) {
        while (order.qty > 0 && !bids_.empty() && bids_.rbegin()->first >= order.price) {
            auto& level = bids_.rbegin()->second;
            while (order.qty > 0 && !level.empty()) {
                Order& resting = level.front();
                Quantity fill_qty = std::min(order.qty, resting.qty);
                fills.push_back({order.id, resting.id, resting.price, fill_qty});
                order.qty -= fill_qty;
                resting.qty -= fill_qty;
                if (resting.qty == 0) level.erase(level.begin());
            }
            if (level.empty()) bids_.erase(std::prev(bids_.end()));
        }
    }

    std::map<Price, std::vector<Order>> bids_;
    std::map<Price, std::vector<Order>> asks_;
};

int main() {
    OrderBook book;
    auto fills = book.add_order(Order(1, Side::Sell, 100, 50));
    std::println("After sell 50@100: best_bid=0 best_ask=100");

    fills = book.add_order(Order(2, Side::Buy, 100, 30));
    for (const auto& f : fills)
        std::println("fill: order={} match={} price={} qty={}", f.order_id, f.match_id, f.price, f.qty);
    std::println("best_ask=100 (20 left)");

    fills = book.add_order(Order(3, Side::Buy, 99, 100));
    std::println("After buy 100@99: best_bid=99");
    std::println("Done. Order book has {} / {}", book.best_bid(), book.best_ask());

    // Benchmark: add_order (limit orders) throughput
    const int bench_orders = 100'000;
    OrderBook bookb;
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < bench_orders; ++i) {
        bookb.add_order(Order(static_cast<OrderId>(i), (i % 2 == 0) ? Side::Buy : Side::Sell, 100 + (i % 10), 100));
    }
    auto t1 = std::chrono::steady_clock::now();
    auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::println("Benchmark: {} add_order in {} us = {:.0f} orders/sec", bench_orders, us, 1e6 * bench_orders / std::max(1.0, static_cast<double>(us)));
    return 0;
}
