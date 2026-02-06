#pragma once

#include <types.h>
#include <order_book.h>
#include <object_pool.h>
#include <unordered_map>
#include <vector>
#include <atomic>

class MatchingEngine {
public:
    MatchingEngine() : order_pool_(100000) {} // Pre-allocate pool for 100k orders

    //Main Operations
    std::vector<Trade> process_new_order(const Order& order);

    //PreTrade Validations
    bool validate_order(const Order& order);

    //Dealloations
    void cancel_order(const OrderID& order_id, const Symbol& symbol);

    //orderbook access
    OrderBook& get_or_create_order_book(const Symbol& symbol);
    OrderBook* get_order_book(const Symbol& symbol);

    //Query Methods
    const std::vector<Trade>& getTradeHistory() const{
        return trade_history_;
    }

    //Stats
    struct Stats {
        std::atomic<uint64_t> total_orders{0};
        std::atomic<uint64_t> total_trades{0};
        std::atomic<uint64_t> total_volume{0};
    };

    const Stats& getStats() const {
        return stats_;
    }
    void printStats() const;
    void resetStats() ;
private:
    ObjectPool<Order> order_pool_; // Pool for managing Order objects
    //OrderBooks mapped by Symbol
    std::unordered_map<Symbol, OrderBook> order_books_;

    //Trade History
    std::vector<Trade> trade_history_;

    //Engine Stats
    Stats stats_;

    //Helper Methods
    std::vector<Trade> match_against_buy_orders(OrderBook& book,Order* sell_order);
    std::vector<Trade> match_against_sell_orders(OrderBook& book, Order* buy_order);

    Trade create_trade(Order* buy_order, Order* sell_order, Quantity trade_quantity, Price trade_price);
};
