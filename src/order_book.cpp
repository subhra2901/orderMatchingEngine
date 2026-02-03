#include <order_book.h>
#include <algorithm>
#include <iostream>

#include<../logging/logger.hpp>

static Logger logger("order_book");

OrderBook::OrderBook(const Symbol& symbol) : symbol_(symbol) {
    LOG_INFO(logger, "OrderBook created for symbol: " + symbol);
}

void OrderBook::add_order(const Order& order) {
    LOG_DEBUG(logger, "Adding order ID: " + std::to_string(order.id) + " to OrderBook for symbol: " + symbol_);
    
    auto& price_map = (order.side == OrderSide::BUY) ? buy_orders_ : sell_orders_;
    auto& orderlist = price_map[order.price];

    size_t position = orderlist.size();
    orderlist.push_back(order);

    order_lookup_[order.id] = {order.side, order.price, std::prev(orderlist.end())};

    LOG_DEBUG(logger, "Order ID: " + std::to_string(order.id) + " added at price: " + std::to_string(order.price) + 
              " with position: " + std::to_string(position));
    //1. Add order to appropriate book (buy/sell)
    //2. Update order_lookup_ for quick access
}

bool OrderBook::cancel_order(const OrderID& order_id) {
    auto it = order_lookup_.find(order_id);
    if (it == order_lookup_.end()) {
        LOG_WARN(logger, "Attempted to cancel non-existent order ID: " + std::to_string(order_id));
        return false; // Order not found
    }
    OrderInfo info = it->second;
    auto& price_map = (info.side == OrderSide::BUY) ? buy_orders_: sell_orders_;
    auto orderlist_it = price_map.find(info.price);
    if (orderlist_it != price_map.end()) {
        orderlist_it->second.erase(info.iterator);
        if (orderlist_it->second.empty()) {
            price_map.erase(orderlist_it);
        }
        order_lookup_.erase(order_id);
        LOG_INFO(logger, "Cancelled order ID: " + std::to_string(order_id));
        return true;
    }   
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



