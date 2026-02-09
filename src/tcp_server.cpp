#include <tcp_server.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <vector>
#include <iostream>

TcpServer::TcpServer(int port) : port_(port), running_(false) {
    serverFd_ = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;  
    addr.sin_port = htons(port);

    bind(serverFd_, (sockaddr*)&addr, sizeof(addr));
    listen(serverFd_, 10);

    fcntl(serverFd_, F_SETFL, O_NONBLOCK);
}

TcpServer::~TcpServer() {
    running_ = false;
    close(serverFd_);
}

void TcpServer::sendPacket(int fd, const char* data, size_t len) {
    send(fd, data, len, 0);
}

void TcpServer::start() {
    running_ = true;
    std::vector<int> clients;
    char buffer[1024];

    std::cout << "Server started on port " << port_ << std::endl;

    while (running_) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(serverFd_, &fds);
        int max_fd = serverFd_;

        for (int client_fd : clients) {
            FD_SET(client_fd, &fds);
            if (client_fd > max_fd) {
                max_fd = client_fd;
            }
        }
        timeval tv{0, 100}; // 100ms
        int activity = select(max_fd + 1, &fds, nullptr, nullptr, &tv);
        if (activity < 0) {
            continue;
        }
        if (FD_ISSET(serverFd_, &fds)) {
            int new_client = accept(serverFd_, nullptr, nullptr);
            if (new_client >= 0) {
                std::cout << "New client connected: " << new_client << std::endl;
                fcntl(new_client, F_SETFL, O_NONBLOCK);
                clients.push_back(new_client);
                if (onConnection_) {
                    onConnection_(new_client);
                }
            }
        }
            for (auto it = clients.begin(); it != clients.end();) {
                int client_fd = *it;
                if (FD_ISSET(client_fd, &fds)) {
                    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
                    std::cout << "Received data from client " << client_fd << ": " << bytes_read << " bytes" << std::endl;
                    if (bytes_read > 0) {
                        if (onMessage_) {
                            onMessage_(client_fd, buffer, bytes_read);
                        }
                        ++it;
                    } 
                    else if (bytes_read == 0) {
                        if (onDisconnection_) {
                            onDisconnection_(client_fd);
                        }
                        close(client_fd);
                        it = clients.erase(it);
                    }
                    else{
                        if(errno != EWOULDBLOCK && errno != EAGAIN) {
                            if (onDisconnection_) {
                                onDisconnection_(client_fd);
                            }
                            close(client_fd);
                            it = clients.erase(it);
                        } else {
                            ++it;
                        }
                    }
                } 
                else {
                    ++it;
                }
            }
        
    }

}