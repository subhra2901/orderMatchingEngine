#include <client_gateway.h>
#include <../logging/logger.hpp>
#include <cstring>
#include <iostream>
#include <algorithm>

ClientGateway::ClientGateway(MatchingEngine& engine, TcpServer& server) : engine_(engine), server_(server) {
    server_.setOnMessage([this](int fd, const char* data, size_t len) {
        this->onMessage(fd, data, len);
    });
    server_.setOnConnection([this](int fd) {
        this->onConnection(fd);
    });
    server_.setOnDisconnection([this](int fd) {
        this->onDisconnection(fd);
    });
}

void ClientGateway::onConnection(int fd) {
    LOG_INFO("Client connected: " + std::to_string(fd));
    sessions_[fd] = Session();
    std::cout << "Client " << fd << " connected" << std::endl;
}

void ClientGateway::onDisconnection(int fd) {
    LOG_INFO("Client disconnected: " + std::to_string(fd));
    sessions_.erase(fd);
    std::cout << "Client " << fd << " disconnected" << std::endl;
}

void ClientGateway::onMessage(int fd, const char* data, size_t len) {
    std::cout << "Processing message from client " << fd << std::endl;
    if (len < sizeof(MessageHeader)) {
        LOG_WARN("Received message too short from client " + std::to_string(fd));
        return;
    }
    auto* header = reinterpret_cast<const MessageHeader*>(data);

    if( header->type == MessageType::LOGIN_REQUEST) {
        auto* req = reinterpret_cast<const LoginRequest*>(data);
        handleLogin(fd, *req);
    } else if (header->type == MessageType::NEW_ORDER) {
        auto* req = reinterpret_cast<const NewOrderRequest*>(data);
        handleNewOrder(fd, *req);
    } 
    else if (header->type == MessageType::MARKET_DATA_REQUEST) {
        LOG_INFO("Received market data request from client " + std::to_string(fd));
        auto* req = reinterpret_cast<const MarketDataRequest*>(data);
        handleMarketDataRequest(fd, *req);
        // Handle market data request (not implemented in this snippet)
    }
    else {
        LOG_WARN("Received unknown message type from client " + std::to_string(fd));
    }
}

void ClientGateway::handleLogin(int fd, const LoginRequest& req) {
    sessions_[fd].logged_in = true;
    sessions_[fd].user_id = fd; // For simplicity, assign a fixed user

    LoginResponse resp;
    resp.header = {0, MessageType::LOGIN_RESPONSE, sizeof(LoginResponse)};

    resp.status = 1; // Success
    strncpy(resp.message, "Login successful", sizeof(resp.message) - 1);

    server_.sendPacket(fd, reinterpret_cast<const char*>(&resp), sizeof(resp));

    LOG_INFO("Client " + std::to_string(fd) + " logged in as user " + std::to_string(sessions_[fd].user_id));
}

void ClientGateway::handleNewOrder(int fd, const NewOrderRequest& req) {
    if (!sessions_[fd].logged_in) {
        LOG_WARN("Client " + std::to_string(fd) + " attempted to place order without logging in");
        return;
    }

    // Safely construct symbol string from fixed-size char array and trim padding
    std::string symbol(req.symbol, sizeof(req.symbol));
    while (!symbol.empty() && (symbol.back() == '\0' || symbol.back() == ' ')) symbol.pop_back();

    Order order;
    order.id = req.client_order_id;
    order.user_id = sessions_[fd].user_id;
    order.symbol = symbol;
    order.side = req.side == 0 ? OrderSide::BUY : OrderSide::SELL;
    order.type = req.type == 0 ? OrderType::MARKET : OrderType::LIMIT;
    order.price = req.price;
    order.quantity = req.quantity;

    LOG_INFO("Client " + std::to_string(fd) + " placed new order: " + std::to_string(order.id));

    auto trades = engine_.process_new_order(order);

    if(!trades.empty()) {
        for (const auto& trade : trades) {
            order.reduce_quantity(trade.quantity); // Update filled quantity
            ExecutionReport report;
            report.header = {0, MessageType::EXECUTION_REPORT, sizeof(ExecutionReport)};
            report.client_order_id = order.id;
            report.execution_id = trade.buy_order_id; // For simplicity, use buy order ID as execution ID
            strncpy(report.symbol, trade.symbol.c_str(), sizeof(report.symbol) - 1);
            report.side = order.side == OrderSide::BUY ? 0 : 1;
            report.price = trade.price;
            report.quantity = trade.quantity;
            report.filled_quantity = order.quantity_filled;
            report.status = order.is_filled() ? 2 : 1; // 2=Filled, 1=Partially Filled

            server_.sendPacket(fd, reinterpret_cast<const char*>(&report), sizeof(report));

            LOG_DEBUG("Sent execution report to client " + std::to_string(fd) + " for order " + std::to_string(order.id));
        }
    }
    else {
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
        server_.sendPacket(fd, reinterpret_cast<const char*>(&report), sizeof(report));
        LOG_DEBUG("No trades executed for client " + std::to_string(fd) + " order " + std::to_string(order.id));
    }
}
void ClientGateway::handleMarketDataRequest(int fd, const MarketDataRequest& req) {
    // Safely construct symbol string and trim trailing spaces / nulls
    std::string symbol(req.symbol, sizeof(req.symbol));
    while (!symbol.empty() && (symbol.back() == '\0' || symbol.back() == ' ')) symbol.pop_back();
    LOG_INFO("Received market data request for symbol " + symbol + " from client " + std::to_string(fd));
    OrderBook* book = engine_.get_order_book(symbol);
    MarketDataSnapshot snapshot;
    snapshot.header = {0, MessageType::MARKET_DATA_SNAPSHOT, sizeof(MarketDataSnapshot)};
    strncpy(snapshot.symbol, symbol.c_str(), sizeof(snapshot.symbol) - 1);
    snapshot.num_bids = 0;
    snapshot.num_asks = 0;
    if (!book) {
        LOG_WARN("No order book found for symbol " + symbol);
        return;
    }
    else{
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
    server_.sendPacket(fd, reinterpret_cast<const char*>(&snapshot), sizeof(snapshot));

}
   
