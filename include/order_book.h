#pragma once

#include <types.h>
#include <map>
#include <deque>
#include <unordered_map>
#include <vector>
#include <memory>

namespace exchange {

class OrderBook {
public:
    //Constructor
    explicit OrderBook(const std::string& symbol);

    //core methods
    void add_order(const Order& order);
    bool cancel_order(const OrderID& order_id);
    bool modifyOrderQuantity(const OrderID& order_id, Quantity new_quantity);

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
    std::map<Price, std::deque<Order>, std::greater<Price>> buy_orders_;

    // Sell Orders: Price -> Deque of Orders (sorted ascending)
    std::map<Price, std::deque<Order>> sell_orders_;

    //Order ID to OrderInfo mapping for quick access
    std::unordered_map<OrderID, OrderInfo> order_lookup_;

    //Helper methods
    Order* getBestOrder(OrderSide side);

    void addToLookup(const Order& order, OrderSide side, Price price, size_t position);

    bool removeFromLookup(const OrderID& order_id);
};
