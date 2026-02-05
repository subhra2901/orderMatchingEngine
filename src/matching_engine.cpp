#include <matching_engine.h>
#include <iostream>
#include <chrono>
#include <../logging/logger.hpp>

std::vector<Trade> MatchingEngine::process_new_order(const Order& incoming_order) {
    std::vector<Trade> trades;
    // 1. Validate the order
    if (!validate_order(incoming_order)) {
        LOG_ERROR("Order ID: " + std::to_string(incoming_order.id) + " failed validation.");
        return trades; // Return empty trade list on invalid order
    }
    // 2. Get or create the order book for the symbol
    OrderBook& book = get_or_create_order_book(incoming_order.symbol);
    // 3. Match the order against existing orders
    if (incoming_order.side == OrderSide::BUY) {
        trades = match_against_sell_orders(book, incoming_order);
    } else {
        trades = match_against_buy_orders(book, incoming_order);
    }
    // 4. Add the order to the book if not fully filled
    if (!incoming_order.is_filled()) {
        book.add_order(incoming_order);
    }
    // 5. Update stats
    stats_.total_orders++;
    // 6 . Return the list of trades executed

    return trades;
}

std::vector<Trade> MatchingEngine::match_against_buy_orders(OrderBook& book, const Order& sell_order) {
    std::vector<Trade> trades;
    Order& sell_order_copy = const_cast<Order&>(sell_order); // Create a modifiable copy
    while (sell_order_copy.remaining_qty() > 0) {
        Order* best_bid = book.getBestBid();
        if (!best_bid || best_bid->price < sell_order_copy.price) {
            break; // No more matching possible
        }
        Quantity trade_qty = std::min(sell_order_copy.remaining_qty(), best_bid->remaining_qty());
        Price trade_price = best_bid->price;
        Trade trade = create_trade(*best_bid, sell_order_copy, trade_qty, trade_price);
        trades.push_back(trade);
        // Update order quantities
        sell_order_copy.reduce_quantity(trade_qty);
        best_bid->reduce_quantity(trade_qty);
        // Remove fully filled orders from the book
        if (best_bid->is_filled()) {
            book.cancel_order(best_bid->id);
        }
    }
    return trades;
}

std::vector<Trade> MatchingEngine::match_against_sell_orders(OrderBook& book, const Order& buy_order) {
    std::vector<Trade> trades;
    Order& buy_order_copy = const_cast<Order&>(buy_order); // Create a modifiable copy
    while (buy_order_copy.remaining_qty() > 0) {
        Order* best_ask = book.getBestAsk();
        if (!best_ask || best_ask->price > buy_order_copy.price) {
            break; // No more matching possible
        }
        Quantity trade_qty = std::min(buy_order_copy.remaining_qty(), best_ask->remaining_qty());
        Price trade_price = best_ask->price;
        Trade trade = create_trade(buy_order_copy, *best_ask, trade_qty, trade_price);
        trades.push_back(trade);
        // Update order quantities
        buy_order_copy.reduce_quantity(trade_qty);
        best_ask->reduce_quantity(trade_qty);
        // Remove fully filled orders from the book
        if (best_ask->is_filled()) {
            book.cancel_order(best_ask->id);
        }
    }
    return trades;
}

Trade MatchingEngine::create_trade(const Order& buy_order, const Order& sell_order, Quantity trade_quantity, Price trade_price) {
    Trade trade{
        .buy_order_id = buy_order.id,
        .sell_order_id = sell_order.id,
        .symbol = buy_order.symbol,
        .price = trade_price,
        .quantity = trade_quantity,
        .timestamp = std::chrono::system_clock::now().time_since_epoch().count()
    };
    //Update stats
    stats_.total_trades++;
    stats_.total_volume += trade_quantity;
    //Add to trade history
    trade_history_.push_back(trade);
    return trade;
}

bool MatchingEngine::validate_order(const Order& order) {
    //check Quantiy > 0, price Valid, type Valid etc.
    if(order.quantity == 0) {
        LOG_ERROR("Invalid order quantity: 0 for order ID: " + std::to_string(order.id));
        return false;
    }
    if(order.symbol.empty()) {
        LOG_ERROR("Invalid order symbol: empty for order ID: " + std::to_string(order.id));
        return false;
    }
    if(order.type == OrderType::LIMIT && order.price <= 0.0) {
        LOG_ERROR("Invalid limit order price: " + std::to_string(order.price) + " for order ID: " + std::to_string(order.id));
        return false;
    }
    if(order.price < 0.0) {
        LOG_ERROR("Invalid order price: " + std::to_string(order.price) + " for order ID: " + std::to_string(order.id));
        return false;
    }
    return true;
}

OrderBook& MatchingEngine::get_or_create_order_book(const Symbol& symbol) {
    auto it = order_books_.find(symbol);
    if (it != order_books_.end()) {
        return it->second;
    }
    it = order_books_.emplace(symbol, OrderBook(symbol)).first;
    return it->second;
}

OrderBook* MatchingEngine::get_order_book(const Symbol& symbol) {
    auto it = order_books_.find(symbol);
    if (it != order_books_.end()) {
        return &it->second;
    }
    return nullptr;
}

void MatchingEngine::printStats() const {
    std::cout << "=== Matching Engine Stats ===\n";
    std::cout << "Total Orders: " << stats_.total_orders.load() << std::endl;
    std::cout << "Total Trades: " << stats_.total_trades.load() << std::endl;
    std::cout << "Total Volume: " << stats_.total_volume.load() << std::endl;
}

void MatchingEngine::resetStats() {
    stats_.total_orders.store(0);
    stats_.total_trades.store(0);
    stats_.total_volume.store(0);
}



