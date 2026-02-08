#include <tcp_server.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <vector>
#include <iostream>

TcpServer::TcpServer(int port) : port(port), running(false) {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;  
    addr.sin_port = htons(port);

    bind(server_fd, (sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 10);

    fcntl(server_fd, F_SETFL, O_NONBLOCK);
}

TcpServer::~TcpServer() {
    stop();
    running = false;
    close(server_fd);
}

void TcpServer::sendPacket(int fd, const char* data, size_t len) {
    send(fd, data, len, 0);
}

void TcpServer::start() {
    running = true;
    std::vector<int> clients;
    char buffer[1024];

    std::cout << "Server started on port " << port_ << std::endl;

    while (running_) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(server_fd, &fds);
        int max_fd = server_fd;

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
        if (FD_ISSET(server_fd, &fds)) {
            int new_client = accept(server_fd, nullptr, nullptr);
            if (new_client >= 0) {
                fcntl(new_client, F_SETFL, O_NONBLOCK);
                clients.push_back(new_client);
                if (onConnection_) {
                    onConnection_(new_client);
                }
            }

            for (auto it = clients.begin(); it != clients.end();) {
                int client_fd = *it;
                if (FD_ISSET(client_fd, &fds)) {
                    ssize_t bytes_read = recv(client_fd, buffer, sizeof(buffer), 0);
                    if (bytes_read > 0) {
                        if (onMessage_) {
                            onMessage_(client_fd, buffer, bytes_read);
                        }
                    } else {
                        if (onDisconnection_) {
                            onDisconnection_(client_fd);
                        }
                        close(client_fd);
                        it = clients.erase(it);
                    }
                } else {
                    ++it;
                }
            }
        }
    }

}