#include "../logging/logger.hpp"
#include <chrono>
#include <client_gateway.h>
#include <iostream>
#include <matching_engine.h>
#include <order.h>
#include <tcp_server.h>

int main(int argc, char *argv[]) {
  int port = 8080;

  if (argc > 1) {
    port = std::stoi(argv[1]);
  }

  auto &logger = Logger::getInstance("MatchingEngineMain");

  for (int i = 0; i < argc; i++) {
    std::string arg(argv[i]);
    if (arg == "--log-level" && i + 1 < argc) {
      LogLevel level = (argv[i + 1] == std::string("DEBUG"))  ? LogLevel::DEBUG
                       : (argv[i + 1] == std::string("INFO")) ? LogLevel::INFO
                       : (argv[i + 1] == std::string("WARN")) ? LogLevel::WARN
                                                              : LogLevel::ERROR;
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