#include "Logger.hpp"
#include <chrono>

#ifndef PROJECT_ROOT_PATH
    #define PROJECT_ROOT_PATH "."
#endif

Logger::Logger(const std::string& process_name) : proc_name_(process_name) {
    namespace fs = std::filesystem;
    fs::path root(PROJECT_ROOT_PATH);
    fs::path logs_dir = root / "logs";
    fs::create_directories(logs_dir);

    // Create a unique timestamp for the filename (seconds + ms)
    auto now = std::chrono::system_clock::now();
    std::string timestamp = std::format("{:%Y-%m-%d_%H-%M-%OS}", now);
    
    std::replace(timestamp.begin(), timestamp.end(), ':', '-');

    std::string filename = std::format("{}_{}.log", proc_name_, timestamp);
    file_.open(logs_dir / filename, std::ios::out);
}

Logger::~Logger() {
    if (file_.is_open()) file_.close();
}

LogLevel Logger::stringToLevel(std::string str) {
    for (auto& c : str) c = (char)std::tolower(c);
    if (str == "debug") return LogLevel::DEBUG;
    if (str == "warn")  return LogLevel::WARN;
    if (str == "error") return LogLevel::ERROR;
    return LogLevel::INFO;
}

void Logger::log(LogLevel level, const std::string& msg, const std::source_location loc) {
    if (level < min_level_) return;

    std::lock_guard<std::mutex> lock(mutex_);
    if (!file_.is_open()) return;

    auto now = std::chrono::system_clock::now();
    
    
    std::string_view lvl_str;
    switch (level) {
        case LogLevel::DEBUG: lvl_str = "DEBUG"; break;
        case LogLevel::INFO:  lvl_str = "INFO "; break;
        case LogLevel::WARN:  lvl_str = "WARN "; break;
        case LogLevel::ERROR: lvl_str = "ERROR"; break;
    }

    file_ << std::format("{:%H:%M:%S} | {} | L:{:<4} | {:<20} | {:<15} | {}\n",
                         now,
                         lvl_str,
                         loc.line(),
                         loc.function_name(),
                         std::filesystem::path(loc.file_name()).filename().string(),
                         msg);
    file_.flush();
}