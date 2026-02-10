#pragma once

#include <matching_engine.h>
#include <tcp_server.h>
#include <protocol.h>
#include <unordered_map>
#include <fstream>
#include <../logging/logger.hpp>

class ClientGateway {
public:
    ClientGateway(MatchingEngine& engine, TcpServer& server);
    ~ClientGateway();

    void replayEvents();
    void startLogging() {
        std::filesystem::path log_dir = std::filesystem::path(PROJECT_ROOT_PATH) / "bins";
        if (!std::filesystem::exists(log_dir)) {
            std::filesystem::create_directories(log_dir);
        }
        event_log_.open((log_dir / "orders.bin").string(), std::ios::binary | std::ios::app);
        if(!event_log_.is_open()) {
            LOG_ERROR("Failed to open event log file for writing");
        }
    }
private:
    // TcpServer callbacks
    void onMessage(int fd, const char* data, size_t len);
    void onConnection(int fd);
    void onDisconnection(int fd);

    // Message handlers
    void handleLogin(int fd, const LoginRequest& req);
    void handleNewOrder(int fd, const NewOrderRequest& req);
    void handleMarketDataRequest(int fd, const MarketDataRequest& req);
    void handleNewOrderInternal(const NewOrderRequest& req, int user_id,int fd,bool is_replay = false);

    // Session information
    struct Session {
        bool logged_in = false;
        int user_id = 0;
    };

    TcpServer& server_;
    MatchingEngine& engine_;
    std::unordered_map<int, Session> sessions_;

    std::ofstream event_log_;
};