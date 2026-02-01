#include <matching_engine.h>
#include <iostream>

std::vector<Trade> MatchingEngine::process_new_order(const Order& incoming_order) {
    std::vector<Trade> trades;
    //TODO: Implement order processing logic
    // 1. Validate the order
    // 2. Get or create the order book for the symbol
    // 3. Match the order against existing orders
    // 4. Add the order to the book if not fully filled
    // 5. Update trade history and stats
    // 6 . Return the list of trades executed
    return trades;
}

bool MatchingEngine::validate_order(const Order& order) {
    //TODO: Implement order validation logic
    //check Quantiy > 0, price Valid, type Valid etc.
    return order.quantity >0 && (order.type == OrderType::MARKET || order.price >0);
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



