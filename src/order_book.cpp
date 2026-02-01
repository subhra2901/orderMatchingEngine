#include <order_book.h>
#include <algorithm>
#include <iostream>

OrderBook::OrderBook(const Symbol& symbol) : symbol_(symbol) {}

void OrderBook::add_order(const Order& order) {
    // To be implemented
    //1. Add order to appropriate book (buy/sell)
    //2. Update order_lookup_ for quick access
}

bool OrderBook::cancel_order(const OrderID& order_id) {
    // To be implemented
    //1. Find order in order_lookup_
    //2. Remove from appropriate book (buy/sell)
    //3. Remove from order_lookup_
    return false;
}

bool OrderBook::modifyOrderQuantity(const OrderID& order_id, Quantity new_quantity) {
    // To be implemented
    //1. Find order 
    //2. Modify quantity only if reducing
    return false;
}

Order* OrderBook::getBestBid() {
    //TODO: Implement this method
    // Return pointer to best bid order
    return nullptr;
}

Order* OrderBook::getBestAsk() {
    //TODO : Implement this method
    // Return pointer to best ask order
    return nullptr;
}

Price OrderBook::getSpread() {
    // TODO: Implement this method
    // Return ask - bid (0 if no bids or asks)
    return 0.0;
}

L1Quote OrderBook::getL1Quote() {
    //TODO: Implement this method
    return L1Quote{};
}

L2Quote OrderBook::getL2Quote(size_t depth) const {
    //TODO: Implement this method
    return L2Quote{};
}

size_t OrderBook::getBuyOrders() const {
    size_t total = 0;
    for (const auto& [price, orders] : buy_orders_) {
        total += orders.size();
    }
    return total;
}
size_t OrderBook::getSellOrders() const {
    size_t total = 0;
    for (const auto& [price, orders] : sell_orders_) {
        total += orders.size();
    }
    return total;
}
size_t OrderBook::getTotalOrders() const {
    return getBuyOrders() + getSellOrders();
}



