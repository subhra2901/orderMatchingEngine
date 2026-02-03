#include "logger.hpp"
#include <filesystem>
#include <ctime>
#include <chrono>
#include <iomanip>
#include <sstream>


#ifndef PROJECT_ROOT_PATH
    #define PROJECT_ROOT_PATH "."
#endif

std::string Logger::getLogFilename(const std::string& base_name) const {
    
    std::filesystem::path root(PROJECT_ROOT_PATH);
    std::filesystem::path log_path = root / "logs";
    
    
    std::filesystem::create_directories(log_path);

    std::filesystem::path full_path = log_path / (base_name + "_" + getTimestamp(true) + ".log");
    return full_path.string();
}

std::string Logger::getTimestamp(bool filename_format) const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    
    std::tm tm = *std::localtime(&time_t);
    
    std::stringstream ss;
    if (filename_format) {
        ss << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S"); 
    } else {
        ss << std::put_time(&tm, "%H:%M:%S");          
    }
    return ss.str();
}

Logger::Logger(const std::string& base_name) {
    std::string filename = getLogFilename(base_name);
    file_.open(filename, std::ios::trunc); 
    
    if (file_.is_open()) {
        file_ << "=== Logger Started: " << getTimestamp(false) 
              << " | File: " << filename << " ===\n";
        file_.flush();
    }
}

Logger::~Logger() {
    if (file_.is_open()) {
        file_.close();
    }
}

void Logger::log(LogLevel level, const std::string& file, int line, 
                const std::string& func, const std::string& msg) {
    if (level < min_level_) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    if (file_.is_open()) {
        // Find last path separator to show only the filename
        size_t last_slash = file.find_last_of("/\\");
        std::string short_file = (last_slash == std::string::npos) ? file : file.substr(last_slash + 1);

        file_ << getTimestamp() << " [" 
              << (level == LogLevel::INFO ? "INFO" : 
                  level == LogLevel::DEBUG ? "DEBUG" :
                  level == LogLevel::WARN ? "WARN" : "ERROR")
              << "] " << short_file << ":" << line << " " << func << ": " << msg << "\n";
        file_.flush();
    }
}