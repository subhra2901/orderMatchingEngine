#pragma once

#include <functional>
#include <atomic>

class TcpServer {
public:
    using OnMessage = std::function<void(int fd, const char* data, size_t len)>;
    using OnConnection = std::function<void(int fd)>;
    using OnDisconnection = std::function<void(int fd)>;

    TcpServer(int port);
    ~TcpServer();

    void start();
    void sendPacket(int fd, const char* data, size_t len);

    void setOnMessage(const OnMessage& callback) { onMessage_ = callback; }
    void setOnConnection(const OnConnection& callback) { onConnection_ = callback; }
    void setOnDisconnection(const OnDisconnection& callback) { onDisconnection_ = callback; }

private:
    int serverFd_;
    int port_;
    std::atomic<bool> running_;
    OnMessage onMessage_;
    OnConnection onConnection_;
    OnDisconnection onDisconnection_;
};