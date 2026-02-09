#include <matching_engine.h>
#include <order.h>
#include <iostream>
#include <chrono>
#include <client_gateway.h>
#include <tcp_server.h>

#include "../logging/logger.hpp"


int main(int argc, char* argv[]) {

    int port = 8080;

    if(argc > 1) {
        port = std::stoi(argv[1]);
    }

    auto& logger = Logger::getInstance("MatchingEngineMain");

    for(int i=0; i<argc; i++) {
        std::string arg(argv[i]);
        if(arg == "--log-level" && i+1 < argc) {
            LogLevel level = argv[i+1] == "DEBUG" ? LogLevel::DEBUG :
                             argv[i+1] == "INFO"  ? LogLevel::INFO  :
                             argv[i+1] == "WARN"  ? LogLevel::WARN  : LogLevel::ERROR;
            logger.setMinLevel(level);
            i++; // Skip next argument since it's the log level
        } 
    }
    
    std::cout << "Starting Matching Engine on port " << port << std::endl;

    TcpServer server(port);
    MatchingEngine engine;
    ClientGateway gateway(engine, server);

    std::cout << "Server is running..." << std::endl;
    server.start();

    return 0;
}