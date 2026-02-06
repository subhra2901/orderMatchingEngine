#include <matching_engine.h>
#include <order.h>
#include <iostream>
#include <chrono>

#include "../logging/logger.hpp"


int main(int argc, char* argv[]) {
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
    MatchingEngine engine;
    
    LOG_INFO("Initialized Matching Engine");

    std::cout << "Starting Matching Engine..." << std::endl;

    //Create some test orders
    Order sell{
        .id = 1,
        .symbol = "AAPL",
        .side = OrderSide::SELL,
        .type = OrderType::LIMIT,
        .price = 150.0,
        .quantity = 100,
        .timestamp = std::chrono::system_clock::now().time_since_epoch().count()
    };
    Order buy{
        .id = 2,
        .symbol = "AAPL",
        .side = OrderSide::BUY,
        .type = OrderType::LIMIT,
        .price = 150.0,
        .quantity = 50,
        .timestamp = std::chrono::system_clock::now().time_since_epoch().count()
    };
    std::cout << sell << std::endl;
    std::cout << buy << std::endl;

    //Process orders
    auto trades1 = engine.process_new_order(sell);
    auto trades2 = engine.process_new_order(buy);

    std::cout << "Trades Generated from Sell Order:" << trades1.size() << std::endl;
    std::cout << "Trades Generated from Buy Order:" << trades2.size() << std::endl;

    engine.printStats();

    LOG_INFO("Shutting down Matching Engine");

    return 0;

}