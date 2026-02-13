#include <../logging/logger.hpp>
#include <chrono>
#include <iostream>
#include <optional>
#include <matching_engine.h>

std::vector<Trade>
MatchingEngine::process_new_order(const Order &incoming_order) {
  Order *order_ptr = order_pool_.allocate();
  if (!order_ptr) {
    LOG_ERROR("Order pool exhausted. Cannot process new order ID: " +
              std::to_string(incoming_order.id));
    return {};
  }
  *order_ptr = incoming_order;

  std::vector<Trade> trades;
  // 1. Validate the order
  if (!validate_order(*order_ptr)) {
    LOG_ERROR("Order ID: " + std::to_string(incoming_order.id) +
              " failed validation.");
    order_pool_.deallocate(
        order_ptr); // Deallocate the order if validation fails
    return trades;  // Return empty trade list on invalid order
  }
  // 2. Get or create the order book for the symbol
  OrderBook &book = get_or_create_order_book(incoming_order.symbol);

  // Check for sufficient liquidity for IOC and FOK orders
  if (order_ptr->type == OrderType::FOK &&
      !can_fill_completely(book, *order_ptr)) {
    LOG_INFO("Order ID: " + std::to_string(incoming_order.id) +
             " cannot be fully filled. Cancelling.");
    order_pool_.deallocate(order_ptr);
    return trades;
  }

  // 3. Match the order against existing orders
  if (order_ptr->side == OrderSide::BUY) {
    trades = match_against_sell_orders(book, order_ptr);
  } else {
    trades = match_against_buy_orders(book, order_ptr);
  }
  // 4. Add the order to the book if not fully filled
  if (!order_ptr->is_filled()) {
    if (order_ptr->type == OrderType::IOC) {
      LOG_INFO("Order ID: " + std::to_string(incoming_order.id) +
               " is IOC and not fully filled. Cancelling remaining quantity.");
      order_pool_.deallocate(order_ptr); // Deallocate the unfilled IOC order
      return trades;
    }
    book.add_order(order_ptr);
    stats_.total_orders++;
  } else {
    order_pool_.deallocate(order_ptr); // Deallocate if fully filled
  }

  // 5. Return the list of trades executed

  return trades;
}

std::vector<Trade> MatchingEngine::match_against_buy_orders(OrderBook &book,
                                                            Order *sell_order) {
  std::vector<Trade> trades;

  while (sell_order->remaining_qty() > 0) {
    Order *best_bid = book.getBestBid();
    if (!best_bid || best_bid->price < sell_order->price) {
      break; // No more matching possible
    }
    Quantity trade_qty =
        std::min(sell_order->remaining_qty(), best_bid->remaining_qty());
    Price trade_price = best_bid->price;
    Trade trade = create_trade(best_bid, sell_order, trade_qty, trade_price);
    trades.push_back(trade);
    // Update order quantities
    sell_order->reduce_quantity(trade_qty);
    best_bid->reduce_quantity(trade_qty);
    // Remove fully filled orders from the book
    if (best_bid->is_filled()) {
      book.cancel_order(best_bid->id);
      order_pool_.deallocate(best_bid); // Deallocate the fully filled bid order
    }
  }
  return trades;
}

std::vector<Trade> MatchingEngine::match_against_sell_orders(OrderBook &book,
                                                             Order *buy_order) {
  std::vector<Trade> trades;

  while (buy_order->remaining_qty() > 0) {
    Order *best_ask = book.getBestAsk();
    if (!best_ask || best_ask->price > buy_order->price) {
      break; // No more matching possible
    }
    Quantity trade_qty =
        std::min(buy_order->remaining_qty(), best_ask->remaining_qty());
    Price trade_price = best_ask->price;
    Trade trade = create_trade(buy_order, best_ask, trade_qty, trade_price);
    trades.push_back(trade);
    // Update order quantities
    buy_order->reduce_quantity(trade_qty);
    best_ask->reduce_quantity(trade_qty);
    // Remove fully filled orders from the book
    if (best_ask->is_filled()) {
      book.cancel_order(best_ask->id);
      order_pool_.deallocate(best_ask); // Deallocate the fully filled ask order
    }
  }
  return trades;
}

Trade MatchingEngine::create_trade(Order *buy_order, Order *sell_order,
                                   Quantity trade_quantity, Price trade_price) {
  Trade trade{.buy_order_id = buy_order->id,
              .buy_user_id = buy_order->user_id,
              .sell_order_id = sell_order->id,
              .sell_user_id = sell_order->user_id,
              .symbol = buy_order->symbol,
              .price = trade_price,
              .quantity = trade_quantity,
              .timestamp =
                  std::chrono::system_clock::now().time_since_epoch().count()};
  // Update stats
  stats_.total_trades++;
  stats_.total_volume += trade_quantity;
  // Add to trade history
  trade_history_.push_back(trade);
  return trade;
}

bool MatchingEngine::validate_order(const Order &order) {
  // check Quantiy > 0, price Valid, type Valid etc.
  if (order.quantity == 0) {
    LOG_ERROR("Invalid order quantity: 0 for order ID: " +
              std::to_string(order.id));
    return false;
  }
  if (order.symbol.empty()) {
    LOG_ERROR("Invalid order symbol: empty for order ID: " +
              std::to_string(order.id));
    return false;
  }
  if (order.type == OrderType::LIMIT && order.price <= 0.0) {
    LOG_ERROR("Invalid limit order price: " + std::to_string(order.price) +
              " for order ID: " + std::to_string(order.id));
    return false;
  }
  if (order.price < 0.0) {
    LOG_ERROR("Invalid order price: " + std::to_string(order.price) +
              " for order ID: " + std::to_string(order.id));
    return false;
  }
  return true;
}

bool MatchingEngine::can_fill_completely(const OrderBook &book,
                                         const Order &order) {
  Quantity needed_qty = order.quantity;
  auto &map =
      (order.side == OrderSide::BUY) ? book.sell_orders_ : book.buy_orders_;
  if (OrderSide::BUY == order.side) {
    for (const auto &[price, orders_at_price] : map) {
      if (price > order.price) {
        break; // No more matching possible
      }
      for (const auto &o : orders_at_price) {
        needed_qty -= o->remaining_qty();
        if (needed_qty <= 0) {
          return true; // Sufficient liquidity found
        }
      }
    }
  } else {
    for (const auto &[price, orders_at_price] : map) {
      if (price < order.price) {
        break; // No more matching possible
      }
      for (const auto &o : orders_at_price) {
        needed_qty -= o->remaining_qty();
        if (needed_qty <= 0) {
          return true; // Sufficient liquidity found
        }
      }
    }
  }
  return false; // Not enough liquidity
}

OrderBook &MatchingEngine::get_or_create_order_book(const Symbol &symbol) {
  auto it = order_books_.find(symbol);
  if (it != order_books_.end()) {
    return it->second;
  }
  it = order_books_.emplace(symbol, OrderBook(symbol)).first;
  return it->second;
}

OrderBook *MatchingEngine::get_order_book(const Symbol &symbol) {
  auto it = order_books_.find(symbol);
  if (it != order_books_.end()) {
    return &it->second;
  }
  return nullptr;
}

std::optional<Order> MatchingEngine::cancel_order(const OrderID &id, const Symbol &symbol,int side) {
  auto &book = get_or_create_order_book(symbol);
  Order *ptr = book.cancel_order(id);
  if (ptr) {
    Order cancelled_data = *ptr; // Copy data before deallocation
    order_pool_.deallocate(ptr);
    return cancelled_data;
  }
  return std::nullopt; // Order not found
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
