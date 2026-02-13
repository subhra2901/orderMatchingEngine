#pragma once

#include <atomic>
#include <object_pool.h>
#include <optional>
#include <order_book.h>
#include <types.h>
#include <unordered_map>
#include <vector>

class MatchingEngine {
  public:
    // Pre-allocate pool for 100k orders
    MatchingEngine() : order_pool_(100000) {
    }

    // Main operations
    std::vector<Trade> process_new_order(const Order &order);

    // Pre-trade validations
    bool validate_order(const Order &order);

    // Order management
    std::optional<Order> cancel_order(const OrderID &order_id, const Symbol &symbol, int side);

    // Order book access
    OrderBook &get_or_create_order_book(const Symbol &symbol);
    OrderBook *get_order_book(const Symbol &symbol);

    // Query methods
    const std::vector<Trade> &getTradeHistory() const {
        return trade_history_;
    }

    // Statistics
    struct Stats {
        std::atomic<uint64_t> total_orders{0};
        std::atomic<uint64_t> total_trades{0};
        std::atomic<uint64_t> total_volume{0};
    };

    const Stats &getStats() const {
        return stats_;
    }

    void printStats() const;
    void resetStats();

  private:
    // Pool for managing Order objects
    ObjectPool<Order> order_pool_;

    // Order books mapped by Symbol
    std::unordered_map<Symbol, OrderBook> order_books_;

    // Trade history
    std::vector<Trade> trade_history_;

    // Engine statistics
    Stats stats_;

    // Helper methods
    std::vector<Trade> match_against_buy_orders(OrderBook &book, Order *sell_order);
    std::vector<Trade> match_against_sell_orders(OrderBook &book, Order *buy_order);

    Trade create_trade(Order *buy_order, Order *sell_order, Quantity trade_quantity,
                       Price trade_price);

    // Check if order can be completely filled
    bool can_fill_completely(const OrderBook &book, const Order &order);
};
