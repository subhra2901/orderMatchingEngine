#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <fstream>
#include <mutex>
#include <filesystem>
#include <source_location>
#include <format>

enum class LogLevel { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3 };

class Logger {
public:
    // Singleton access
    static Logger& getInstance(const std::string& process_name = "Process") {
        static Logger instance(process_name);
        return instance;
    }

    void setMinLevel(LogLevel level) { min_level_ = level; }
    static LogLevel stringToLevel(std::string str);

    // C++20 source_location replaces the need for complex macros
    void log(LogLevel level, const std::string& msg, 
             const std::source_location loc = std::source_location::current());

private:
    explicit Logger(const std::string& process_name);
    ~Logger();

    std::ofstream file_;
    std::mutex mutex_;
    LogLevel min_level_ = LogLevel::INFO;
    std::string proc_name_;
};

// Simplified C++20 Macros
#define LOG_DEBUG(msg) Logger::getInstance().log(LogLevel::DEBUG, msg)
#define LOG_INFO(msg)  Logger::getInstance().log(LogLevel::INFO,  msg)
#define LOG_WARN(msg)  Logger::getInstance().log(LogLevel::WARN,  msg)
#define LOG_ERROR(msg) Logger::getInstance().log(LogLevel::ERROR, msg)

#endif