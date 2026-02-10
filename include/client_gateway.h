#pragma once

#include <matching_engine.h>
#include <tcp_server.h>
#include <protocol.h>
#include <unordered_map>

class ClientGateway {
public:
    ClientGateway(MatchingEngine& engine, TcpServer& server);

private:
    // TcpServer callbacks
    void onMessage(int fd, const char* data, size_t len);
    void onConnection(int fd);
    void onDisconnection(int fd);

    // Message handlers
    void handleLogin(int fd, const LoginRequest& req);
    void handleNewOrder(int fd, const NewOrderRequest& req);
    void handleMarketDataRequest(int fd, const MarketDataRequest& req);

    // Session information
    struct Session {
        bool logged_in = false;
        int user_id = 0;
    };

    TcpServer& server_;
    MatchingEngine& engine_;
    std::unordered_map<int, Session> sessions_;
};