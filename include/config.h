#pragma once
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>


class Config {
  public:
    static Config &getInstance() {
        static Config instance;
        return instance;
    }

    Config(const Config &)            = delete;
    Config &operator=(const Config &) = delete;

    // Default values
    int port              = 8080;
    std::string log_level = "INFO";
    bool replay_mode      = false;

    void parseArgs(int argc, char *argv[]) {
        for (int i = 0; i < argc; i++) {
            std::string arg(argv[i]);
            if (arg == "--port" && i + 1 < argc) {
                port = std::stoi(argv[i + 1]);
                i++;
            } else if (arg == "--log-level" && i + 1 < argc) {
                log_level = argv[i + 1];
                i++;
            } else if (arg == "--replay-mode") {
                replay_mode = true;
            } else if (arg == "--help") {
                printHelp();
            }
        }
    }

  private:
    Config() = default;
    void printHelp() {
        std::cout << "Usage: matching_engine [options]\n"
                  << "Options:\n"
                  << "  --port <port>           Set the server port (default: 8080)\n"
                  << "  --log-level <level>    Set log level (DEBUG, INFO, WARN, ERROR)\n"
                  << "  --replay-mode          Enable replay mode to process historical events\n"
                  << "  --help                 Show this help message\n";
        exit(0);
    }
};