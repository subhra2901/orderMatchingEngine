#include <client_gateway.h>
#include <../logging/logger.hpp>
#include <cstring>
#include <iostream>

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
    } else {
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

    Order order;
    order.id = req.client_order_id;
    order.symbol = req.symbol;
    order.side = req.side == 0 ? OrderSide::BUY : OrderSide::SELL;
    order.type = req.type == 0 ? OrderType::MARKET : OrderType::LIMIT;
    order.price = req.price;
    order.quantity = req.quantity;

    LOG_INFO("Client " + std::to_string(fd) + " placed new order: " + std::to_string(order.id));

    auto trades = engine_.process_new_order(order);

    if(!trades.empty()) {
        for (const auto& trade : trades) {
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