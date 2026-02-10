#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <list>

/* Type Aliases */
using OrderID = uint64_t;      // 64-bit unique identifier for an order
using UserID = uint64_t;       // 64-bit unique identifier for a user
using Price = double;          // Price of an order
using Quantity = uint64_t;     // Quantity of an order
using Timestamp = uint64_t;    // Timestamp of an order (nanoseconds since epoch)
using Symbol = std::string;    // Symbol of an order (e.g., "BTCUSD")

/* Enumerations */
enum class OrderSide {
    BUY = 0,    // Buy order
    SELL = 1,   // Sell order
};

enum class OrderType {
    LIMIT = 0,   // Execute at a specific price or better
    MARKET = 1,  // Execute immediately at the best available price
    FOK = 2,     // Fill or Kill: all or nothing order
    IOC = 3,     // Immediate or Cancel: remaining qty will be cancelled
    GFD = 4,     // Good for Day: order remains active until end of day
};

enum class OrderStatus {
    NEW = 0,       // Order is new
    PARTIAL = 1,   // Order has been partially executed
    FILLED = 2,    // Order has been executed
    CANCELLED = 3, // Order has been cancelled
};

/* Core Structures */
struct Order {
    // Identifiers
    OrderID id;
    Symbol symbol;

    // User and Session Info
    UserID user_id;              // ID of the user who placed the order

    // Attributes
    OrderSide side;
    OrderType type;
    Price price;                 // Price per unit (0 for market orders)
    Quantity quantity;           // Quantity of the asset to be traded

    // Execution Tracking
    Quantity quantity_filled = 0;  // Quantity of the asset that has been executed
    OrderStatus status = OrderStatus::NEW;  // Status of the order

    // Timestamps
    Timestamp timestamp;         // Timestamp when created (nanoseconds since epoch)

    // Convenience Methods
    bool is_filled() const { return quantity_filled >= quantity; }

    Quantity remaining_qty() const { return quantity - quantity_filled; }

    void reduce_quantity(Quantity qty) { quantity_filled += qty; }
};

struct Trade {
    OrderID buy_order_id;        // ID of the buyer's order
    UserID buy_user_id;          // ID of the buyer
    OrderID sell_order_id;       // ID of the seller's order
    UserID sell_user_id;         // ID of the seller
    Symbol symbol;               // Symbol of the asset traded
    Price price;                 // Price at which the trade was executed
    Quantity quantity;           // Quantity of the asset traded
    Timestamp timestamp;         // Timestamp when the trade was executed
};

/* Market Data Structures */
struct PriceLevel {
    Price price;
    Quantity bid_qty;
    Quantity ask_qty;
};

struct L1Quote {
    Price bid;                   // Best bid price
    Quantity bid_qty;            // Best bid quantity
    Price ask;                   // Best ask price
    Quantity ask_qty;            // Best ask quantity
};

struct L2Quote {
    std::vector<std::pair<Price, Quantity>> bids;  // Top N bids
    std::vector<std::pair<Price, Quantity>> asks;  // Top N asks
};

// Internal structures
struct OrderInfo {
    Order* order_ptr;                           // Pointer to the actual order in the order book
    std::list<Order*>::iterator list_it;        // Iterator to the order in the price level list
};

