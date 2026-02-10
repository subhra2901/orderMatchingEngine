#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <fstream>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <filesystem>
#include <source_location>
#include <format>
#include <atomic>

enum class LogLevel { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3 };

class Logger {
public:
    static Logger& getInstance(const std::string& process_name = "OME") {
        static Logger instance(process_name);
        return instance;
    }

    void setMinLevel(LogLevel level) { min_level_ = level; }

    void setMinLevel(const std::string& level_str) {
        if (level_str == "DEBUG") min_level_ = LogLevel::DEBUG;
        else if (level_str == "INFO") min_level_ = LogLevel::INFO;
        else if (level_str == "WARN") min_level_ = LogLevel::WARN;
        else if (level_str == "ERROR") min_level_ = LogLevel::ERROR;
    }
    
    // The main entry point: Pushes to queue, doesn't touch the disk
    void log(LogLevel level, std::string msg, 
             const std::source_location loc = std::source_location::current());

private:
    explicit Logger(const std::string& process_name);
    ~Logger();

    void processLogs(); // The background thread function

    // Core Data
    LogLevel min_level_ = LogLevel::INFO;
    std::ofstream file_;
    
    // Async Mechanics
    std::queue<std::string> log_queue_;
    std::mutex queue_mutex_;
    std::condition_variable cv_;
    std::thread worker_thread_;
    std::atomic<bool> exit_flag_{false};
};

#define LOG_DEBUG(msg) Logger::getInstance().log(LogLevel::DEBUG, msg)
#define LOG_INFO(msg)  Logger::getInstance().log(LogLevel::INFO,  msg)
#define LOG_WARN(msg)  Logger::getInstance().log(LogLevel::WARN,  msg)
#define LOG_ERROR(msg) Logger::getInstance().log(LogLevel::ERROR, msg)

#endif