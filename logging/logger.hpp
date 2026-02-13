#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <source_location>
#include <string>
#include <thread>


enum class LogLevel { DEBUG = 0, INFO = 1, WARN = 2, ERROR = 3 };

class Logger {
  public:
    static Logger &getInstance(const std::string &process_name = "OME") {
        static Logger instance(process_name);
        return instance;
    }

    void setMinLevel(LogLevel level) {
        min_level_ = level;
    }

    void setMinLevel(const std::string &level_str) {
        if (level_str == "DEBUG")
            min_level_ = LogLevel::DEBUG;
        else if (level_str == "INFO")
            min_level_ = LogLevel::INFO;
        else if (level_str == "WARN")
            min_level_ = LogLevel::WARN;
        else if (level_str == "ERROR")
            min_level_ = LogLevel::ERROR;
    }

    void log_internal(LogLevel level, std::string msg, const std::source_location loc);
    void flush() {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        cv_.wait(lock, [this]() { return log_queue_.empty(); });
    }
    inline bool should_log(LogLevel level) {
        return level >= min_level_;
    }

  private:
    explicit Logger(const std::string &process_name);
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

class LogStream {
  public:
    LogStream(LogLevel level, std::source_location loc = std::source_location::current())
        : level_(level), loc_(loc) {
    }

    ~LogStream() {
        Logger::getInstance().log_internal(level_, ss_.str(), loc_);
    }
    template <typename T> LogStream &operator<<(const T &value) {
        ss_ << value;
        return *this;
    }
    LogStream &operator<<(std::ostream &(*func)(std::ostream &)) {
        func(ss_);
        return *this;
    }

  private:
    LogLevel level_;
    std::source_location loc_;
    std::ostringstream ss_;
};

#define LOG_DEBUG                                                                                  \
    if (Logger::getInstance().should_log(LogLevel::DEBUG))                                         \
    LogStream(LogLevel::DEBUG)
#define LOG_INFO                                                                                   \
    if (Logger::getInstance().should_log(LogLevel::INFO))                                          \
    LogStream(LogLevel::INFO)
#define LOG_WARN                                                                                   \
    if (Logger::getInstance().should_log(LogLevel::WARN))                                          \
    LogStream(LogLevel::WARN)
#define LOG_ERROR                                                                                  \
    if (Logger::getInstance().should_log(LogLevel::ERROR))                                         \
    LogStream(LogLevel::ERROR)

#endif