#pragma once

#include <types.h>
#include <map>
#include <list>
#include <unordered_map>
#include <vector>
#include <memory>


class OrderBook {

    friend class MatchingEngine; // Allow MatchingEngine to access private members for matching logic

public:
    //Constructor
    explicit OrderBook(const std::string& symbol);

    //core methods
    void add_order(Order* order);
    Order* cancel_order(const OrderID& order_id);
    

    //Query methods
    Order* getBestBid(); //Returns pointer to best bid order
    Order* getBestAsk(); //Returns pointer to best ask order
    Price getSpread(); //Returns the spread between best ask and best bid

    //Market data methods
    L1Quote getL1Quote();
    L2Quote getL2Quote(size_t depth=10) const;

    //Stats
    size_t getTotalOrders() const;
    size_t getBuyOrders() const;
    size_t getSellOrders() const;

    Symbol getSymbol() const{ return symbol_; }
private:
    Symbol symbol_;

    //Order book structures
    // Buy Orders: Price -> Deque of Orders (sorted descending)
    std::map<Price, std::list<Order*>> buy_orders_;

    // Sell Orders: Price -> Deque of Orders (sorted ascending)
    std::map<Price, std::list<Order*>> sell_orders_;

    //Order ID to OrderInfo mapping for quick access
    std::unordered_map<OrderID, OrderInfo> order_lookup_;

    //Helper methods
    Order* getBestOrder(OrderSide side);

};

