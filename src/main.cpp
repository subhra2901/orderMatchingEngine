#include "../logging/logger.hpp"
#include <chrono>
#include <client_gateway.h>
#include <config.h>
#include <iostream>
#include <matching_engine.h>
#include <order.h>
#include <protocol.h>
#include <tcp_server.h>


int main(int argc, char *argv[]) {
    Config::getInstance().parseArgs(argc, argv);
    Logger::getInstance().setMinLevel(Config::getInstance().log_level);

    LOG_INFO << "Starting Matching Engine on port " << Config::getInstance().port;

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