#pragma once

#include <types.h>
#include <order_book.h>
#include <unordered_map>
#include <vector>
#include<atomic>

class MatchingEngine {
public:
    MatchingEngine() = default;

    //Main Operations
    std::vector<Trade> process_new_order(const Order& order);

    //PreTrade Validations
    bool validate_order(const Order& order);

    //orderbook access
    OrderBook& get_or_create_order_book(const Symbol& symbol);
    OrderBook* get_order_book(const Symbol& symbol);

    //Query Methods
    const std::vector<Trade>& getTradeHistory() const{
        return trade_history_;
    }

    //Stats
    struct stats {
        std::atomic<uint64_t> total_orders{0};
        std::atomic<uint64_t> total_trades{0};
        std::atomic<uint64_t> total_volume{0};
    };

    const stats& getStats() const {
        return stats_;
    }
    void printStats() const;
    void resetStats() ;
private:
    //OrderBooks mapped by Symbol
    std::unordered_map<Symbol, OrderBook> order_books_;

    //Trade History
    std::vector<Trade> trade_history_;

    //Engine Stats
    Stats stats_;

    //Helper Methods
    std::vector<Trade> match_against_buy_orders(OrderBook& book, const Order& sell_order);
    std::vector<Trade> match_against_sell_orders(OrderBook& book, const Order& buy_order);

    Trade create_trade(const Order& buy_order, const Order& sell_order, Quantity trade_quantity, Price trade_price);
};

#endif // EXCHANGE_MATCHING_ENGINE_H