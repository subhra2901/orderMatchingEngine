#pragma once

#include <matching_engine.h>
#include <tcp_server.h>
#include <protocol.h>
#include <unordered_map>

class ClientGateway {
public:
    ClientGateway(MatchingEngine& engine, TcpServer& server);

private:
    void onMessage(int fd, const char* data, size_t len);
    void onConnection(int fd);
    void onDisconnection(int fd);
    
    void handleLogin(int fd, const LoginRequest& req);
    void handleNewOrder(int fd, const NewOrderRequest& req);

    struct Session {
        bool logged_in = false;
        int user_id = 0;
    };

    TcpServer& server_;
    MatchingEngine& engine_;
    std::unordered_map<int, Session> sessions_;
};