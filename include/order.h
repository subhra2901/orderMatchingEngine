#pragma once

#include "types.h"
#include <iostream>

// Helper functions for logging and debugging
inline std::string to_string(const OrderSide side) {
    return side == OrderSide::BUY ? "BUY" : "SELL";
}

inline std::string to_string(const OrderType type) {
    switch (type) {
    case OrderType::LIMIT:
        return "LIMIT";
    case OrderType::MARKET:
        return "MARKET";
    case OrderType::IOC:
        return "IOC";
    case OrderType::FOK:
        return "FOK";
    case OrderType::GFD:
        return "GFD";
    default:
        return "UNKNOWN";
    }
}

inline std::string to_string(const OrderStatus status) {
    switch (status) {
    case OrderStatus::NEW:
        return "NEW";
    case OrderStatus::PARTIAL:
        return "PARTIALLY_FILLED";
    case OrderStatus::FILLED:
        return "FILLED";
    case OrderStatus::CANCELLED:
        return "CANCELLED";
    default:
        return "UNKNOWN";
    }
}

inline std::ostream &operator<<(std::ostream &os, const Order &order) {
    os << "Order{ID:" << order.id << ", Symbol:" << order.symbol
       << ", Side:" << to_string(order.side) << ", Type:" << to_string(order.type)
       << ", Price:" << order.price << ", Quantity:" << order.quantity
       << ", Filled:" << order.quantity_filled << ", Status:" << to_string(order.status)
       << ", Timestamp:" << order.timestamp << "}";
    return os;
}
