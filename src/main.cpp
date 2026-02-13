#include "../logging/logger.hpp"
#include <chrono>
#include <client_gateway.h>
#include <iostream>
#include <matching_engine.h>
#include <order.h>
#include <tcp_server.h>
#include <config.h>

int main(int argc, char *argv[]) {
        Config::getInstance().parseArgs(argc, argv);
        Logger::getInstance().setMinLevel(Config::getInstance().log_level);

        std::cout << "Starting Matching Engine on port " << Config::getInstance().port << std::endl;
        
        MatchingEngine engine;
        TcpServer server(Config::getInstance().port);
        ClientGateway gateway(engine, server);

        if (Config::getInstance().replay_mode) {
                LOG_INFO << "Starting in replay mode";
                gateway.replayEvents();
        }
        
        server.start();

        return 0;
}