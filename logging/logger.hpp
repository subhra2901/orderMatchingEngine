#pragma once
#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>

enum class LogLevel { DEBUG, INFO, WARN, ERROR };

class Logger {
public:
    explicit Logger(const std::string& base_name = "ome");
    ~Logger();
    
    void log(LogLevel level, const std::string& file, int line, 
             const std::string& func, const std::string& msg);

private:
    std::ofstream file_;
    LogLevel min_level_ = LogLevel::INFO;
    std::mutex mutex_;
    
    std::string getTimestamp(bool filename_format = false) const;
    std::string getLogFilename(const std::string& base_name) const;
};

#define LOG_INFO(logger, msg)  logger.log(LogLevel::INFO, __FILE__, __LINE__, __func__, msg)
#define LOG_DEBUG(logger, msg) logger.log(LogLevel::DEBUG, __FILE__, __LINE__, __func__, msg)
#define LOG_WARN(logger, msg)  logger.log(LogLevel::WARN, __FILE__, __LINE__, __func__, msg)
#define LOG_ERROR(logger, msg) logger.log(LogLevel::ERROR, __FILE__, __LINE__, __func__, msg)
