#include <../logging/logger.hpp>
#include <algorithm>
#include <client_gateway.h>
#include <config.h>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>

namespace {
std::string clean_symbol(const char *raw, size_t size) {
  std::string s(raw, size);
  // 1. Remove Null Bytes
  s.erase(std::remove(s.begin(), s.end(), '\0'), s.end());
  // 2. Remove Trailing Spaces
  while (!s.empty() && std::isspace(s.back())) {
    s.pop_back();
  }
  return s;
}
} // namespace

ClientGateway::ClientGateway(MatchingEngine &engine, TcpServer &server)
    : engine_(engine), server_(server) {

  startLogging();
  server_.setOnMessage([this](int fd, const char *data, size_t len) {
    this->onMessage(fd, data, len);
  });
  server_.setOnConnection([this](int fd) { this->onConnection(fd); });
  server_.setOnDisconnection([this](int fd) { this->onDisconnection(fd); });
}

void ClientGateway::onConnection(int fd) {
  LOG_INFO("Client connected: " + std::to_string(fd));
  sessions_[fd] = Session();
  std::cout << "Client " << fd << " connected" << std::endl;
}

void ClientGateway::onDisconnection(int fd) {
  LOG_INFO("Client disconnected: " + std::to_string(fd));
  sessions_.erase(fd);

  for (auto &[symbol, subscribers] : market_data_subscriptions_) {
    subscribers.erase(fd);
  }
  std::cout << "Client " << fd << " disconnected" << std::endl;
}

void ClientGateway::onMessage(int fd, const char *data, size_t len) {
  std::cout << "Processing message from client " << fd << std::endl;
  if (len < sizeof(MessageHeader)) {
    LOG_WARN("Received message too short from client " + std::to_string(fd));
    return;
  }
  auto *header = reinterpret_cast<const MessageHeader *>(data);

  if (header->type == MessageType::LOGIN_REQUEST) {
    auto *req = reinterpret_cast<const LoginRequest *>(data);
    handleLogin(fd, *req);
  } else if (header->type == MessageType::NEW_ORDER) {
    auto *req = reinterpret_cast<const NewOrderRequest *>(data);
    handleNewOrder(fd, *req);
  } else if (header->type == MessageType::MARKET_DATA_REQUEST) {
    LOG_INFO("Received market data request from client " + std::to_string(fd));
    auto *req = reinterpret_cast<const MarketDataRequest *>(data);
    handleMarketDataRequest(fd, *req);

  } else if (header->type == MessageType::SUBSCRIPTION_REQUEST) {
    LOG_INFO("Received subscription request from client " + std::to_string(fd));
    auto *req = reinterpret_cast<const SubscriptionRequest *>(data);
    handleSubscriptionRequest(fd, *req);
  } else if (header->type == MessageType::ORDER_CANCEL) {
    LOG_INFO("Received order cancel request from client " + std::to_string(fd));
    auto *req = reinterpret_cast<const OrderCancelRequest *>(data);
    handleOrderCancel(fd, *req);
  } else {
    LOG_WARN("Received unknown message type from client " + std::to_string(fd));
  }
}

void ClientGateway::handleLogin(int fd, const LoginRequest &req) {
  sessions_[fd].logged_in = true;
  sessions_[fd].user_id = fd; // For simplicity, assign a fixed user

  LoginResponse resp;
  resp.header = {0, MessageType::LOGIN_RESPONSE, sizeof(LoginResponse)};

  resp.status = 1; // Success
  strncpy(resp.message, "Login successful", sizeof(resp.message) - 1);

  server_.sendPacket(fd, reinterpret_cast<const char *>(&resp), sizeof(resp));

  LOG_INFO("Client " + std::to_string(fd) + " logged in as user " +
           std::to_string(sessions_[fd].user_id));
}

void ClientGateway::handleNewOrder(int fd, const NewOrderRequest &req) {
  if (!sessions_[fd].logged_in) {
    LOG_WARN("Client " + std::to_string(fd) +
             " attempted to place order without logging in");
    return;
  }

  if (event_log_.is_open()) {
    event_log_.write(reinterpret_cast<const char *>(&req),
                     sizeof(NewOrderRequest));
    event_log_.flush();
  }

  handleNewOrderInternal(req, sessions_[fd].user_id, fd, false);
}
void ClientGateway::handleMarketDataRequest(int fd,
                                            const MarketDataRequest &req) {
  if (!sessions_[fd].logged_in) {
    LOG_WARN("Client " + std::to_string(fd) +
             " attempted to request market data without logging in");
    return;
  }
  // Safely construct symbol string and trim trailing spaces / nulls
  std::string symbol = clean_symbol(req.symbol, sizeof(req.symbol));
  LOG_INFO("Received market data request for symbol " + symbol +
           " from client " + std::to_string(fd));
  OrderBook *book = engine_.get_order_book(symbol);
  MarketDataSnapshot snapshot;
  snapshot.header = {0, MessageType::MARKET_DATA_SNAPSHOT,
                     sizeof(MarketDataSnapshot)};
  strncpy(snapshot.symbol, symbol.c_str(), sizeof(snapshot.symbol) - 1);
  snapshot.num_bids = 0;
  snapshot.num_asks = 0;
  if (!book) {
    LOG_WARN("No order book found for symbol " + symbol);
    return;
  } else {
    auto l2_quote = book->getL2Quote(5); // Get top 5 levels of the order book
    snapshot.num_bids = l2_quote.bids.size();
    snapshot.num_asks = l2_quote.asks.size();
    for (size_t i = 0; i < snapshot.num_bids; ++i) {
      snapshot.bids[i].price = l2_quote.bids[i].first;
      snapshot.bids[i].quantity = l2_quote.bids[i].second;
    }
    for (size_t i = 0; i < snapshot.num_asks; ++i) {
      snapshot.asks[i].price = l2_quote.asks[i].first;
      snapshot.asks[i].quantity = l2_quote.asks[i].second;
    }
  }
  server_.sendPacket(fd, reinterpret_cast<const char *>(&snapshot),
                     sizeof(snapshot));
}

void ClientGateway::handleNewOrderInternal(const NewOrderRequest &req,
                                           int user_id, int fd,
                                           bool is_replay) {
  // This function can be used for both live orders and replayed orders
  // For replayed orders, we might want to skip certain checks or logging
  Order order;
  order.id = req.client_order_id;
  order.user_id = user_id;
  order.symbol = clean_symbol(req.symbol, sizeof(req.symbol));
  order.side = req.side == 0 ? OrderSide::BUY : OrderSide::SELL;
  order.type = req.type == 0 ? OrderType::MARKET : OrderType::LIMIT;
  order.price = req.price;
  order.quantity = req.quantity;

  if (!is_replay) {
    LOG_INFO("Processing new order from client " + std::to_string(fd) + ": " +
             std::to_string(order.id));
  }

  auto trades = engine_.process_new_order(order);

  if (is_replay) {
    LOG_DEBUG("Replayed order " + std::to_string(order.id) + " resulted in " +
              std::to_string(trades.size()) + " trades");
    return; // Don't send execution reports for replayed orders
  }

  if (!trades.empty()) {
    for (const auto &trade : trades) {
      order.reduce_quantity(trade.quantity); // Update filled quantity
      ExecutionReport report;
      report.header = {0, MessageType::EXECUTION_REPORT,
                       sizeof(ExecutionReport)};
      report.client_order_id = order.id;
      report.execution_id =
          trade
              .buy_order_id; // For simplicity, use buy order ID as execution ID
      strncpy(report.symbol, trade.symbol.c_str(), sizeof(report.symbol) - 1);
      report.side = order.side == OrderSide::BUY ? 0 : 1;
      report.price = trade.price;
      report.quantity = trade.quantity;
      report.filled_quantity = order.quantity_filled;
      report.status = order.is_filled() ? 2 : 1; // 2=Filled, 1=Partially Filled

      server_.sendPacket(fd, reinterpret_cast<const char *>(&report),
                         sizeof(report));

      broadcastTradeUpdate(trade); // Broadcast trade update to all clients

      LOG_DEBUG("Sent execution report to client " + std::to_string(fd) +
                " for order " + std::to_string(order.id));
    }
  } else {
    ExecutionReport report;
    report.header = {0, MessageType::EXECUTION_REPORT, sizeof(ExecutionReport)};
    report.client_order_id = order.id;
    report.execution_id = 0; // No execution
    strncpy(report.symbol, order.symbol.c_str(), sizeof(report.symbol) - 1);
    report.side = order.side == OrderSide::BUY ? 0 : 1;
    report.price = order.price;
    report.quantity = order.quantity;
    report.filled_quantity = 0;
    report.status = 0; // 0=New, no execution
    server_.sendPacket(fd, reinterpret_cast<const char *>(&report),
                       sizeof(report));
    LOG_DEBUG("No trades executed for client " + std::to_string(fd) +
              " order " + std::to_string(order.id));
  }
}

void ClientGateway::replayEvents() {
  std::filesystem::path event_log_dir =
      std::filesystem::path(PROJECT_ROOT_PATH) / "bins";
  std::string replay_file = event_log_dir.string() + "/orders.bin";
  std::ifstream infile(replay_file, std::ios::binary);
  if (!infile.is_open()) {
    LOG_ERROR("Failed to open event log file for replay: " + replay_file);
    return;
  }
  LOG_INFO("Replaying events from file: " + replay_file);
  NewOrderRequest req;
  while (infile.read(reinterpret_cast<char *>(&req), sizeof(NewOrderRequest))) {
    handleNewOrderInternal(req, req.client_order_id, -1,
                           true); // user_id and fd are not relevant for replay
  }
  LOG_INFO("Finished replaying events");
}

void ClientGateway::handleSubscriptionRequest(int fd,
                                              const SubscriptionRequest &req) {
  if (!sessions_[fd].logged_in) {
    LOG_WARN("Client " + std::to_string(fd) +
             " attempted to subscribe to market data without logging in");
    return;
  }
  std::string symbol = clean_symbol(req.symbol, sizeof(req.symbol));
  if (req.is_subscribe) {
    market_data_subscriptions_[symbol].insert(fd);
    LOG_INFO("Client " + std::to_string(fd) +
             " subscribed to market data for symbol " + symbol);
  } else {
    market_data_subscriptions_[symbol].erase(fd);
    LOG_INFO("Client " + std::to_string(fd) +
             " unsubscribed from market data for symbol " + symbol);
  }
}

void ClientGateway::broadcastTradeUpdate(const Trade &update) {
  if (market_data_subscriptions_.find(update.symbol) ==
      market_data_subscriptions_.end()) {
    LOG_DEBUG("No subscribers for symbol " + update.symbol +
              ", skipping trade update broadcast");
    return;
  }
  TradeUpdate msg;
  msg.header = {0, MessageType::TRADE_UPDATE, sizeof(TradeUpdate)};
  strncpy(msg.symbol, update.symbol.c_str(), sizeof(msg.symbol) - 1);
  msg.price = update.price;
  msg.quantity = update.quantity;
  msg.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::system_clock::now().time_since_epoch())
                      .count();
  msg.make_side =
      0; // For simplicity, we won't determine maker/taker in this example

  auto &subscribers = market_data_subscriptions_[update.symbol];
  // Broadcast to all connected clients
  for (const auto &fd : subscribers) {
    if (sessions_.find(fd) == sessions_.end() || !sessions_[fd].logged_in) {
      LOG_WARN("Skipping trade update for client " + std::to_string(fd) +
               " because they are not logged in");
      continue;
    }
    server_.sendPacket(fd, reinterpret_cast<const char *>(&msg), sizeof(msg));
  }
}

void ClientGateway::handleOrderCancel(int fd, const OrderCancelRequest &req) {
  if (!sessions_[fd].logged_in) {
    LOG_WARN("Client " + std::to_string(fd) +
             " attempted to cancel order without logging in");
    return;
  }
  if (event_log_.is_open()) {
    event_log_.write(reinterpret_cast<const char *>(&req),
                     sizeof(OrderCancelRequest));
    event_log_.flush();
  }
  std::string symbol = clean_symbol(req.symbol, sizeof(req.symbol));
  std::optional<Order> cancelled_order =
      engine_.cancel_order(req.client_order_id, symbol, req.side);
  LOG_INFO("Processed order cancel request from client " + std::to_string(fd) +
           " for order ID " + std::to_string(req.client_order_id));
  ExecutionReport report;
  report.header = {0, MessageType::EXECUTION_REPORT, sizeof(ExecutionReport)};
  report.client_order_id = req.client_order_id;
  report.execution_id = 0; // No Trade
  strncpy(report.symbol, symbol.c_str(), sizeof(report.symbol) - 1);
  if (cancelled_order) {
    report.side = cancelled_order->side == OrderSide::BUY ? 0 : 1;
    report.price = cancelled_order->price;
    report.quantity = cancelled_order->quantity;
    report.filled_quantity = cancelled_order->quantity_filled;
    report.status = 3; // Canceled
  } else {
    report.side = req.side;
    report.price = 0;
    report.quantity = 0;
    report.filled_quantity = 0;
    report.status = 4; // Reject - Order Not Found }
    LOG_WARN("Order not found for cancellation request from client " + std::to_string(fd) +
             " for order ID " + std::to_string(req.client_order_id));
             
  }
  
  server_.sendPacket(fd, reinterpret_cast<const char *>(&report),
                     sizeof(report));
}

ClientGateway::~ClientGateway() {
  if (event_log_.is_open()) {
    event_log_.close();
  }
}
