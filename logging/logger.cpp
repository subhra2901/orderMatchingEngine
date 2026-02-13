#include "Logger.hpp"
#include <chrono>

#ifndef PROJECT_ROOT_PATH
#define PROJECT_ROOT_PATH "."
#endif

Logger::Logger(const std::string &process_name) {
    std::filesystem::path logs_dir = std::filesystem::path(PROJECT_ROOT_PATH) / "logs";
    std::filesystem::create_directories(logs_dir);

    // Create unique filename with sub-second timestamp
    auto now       = std::chrono::system_clock::now();
    std::string ts = std::format("{:%Y-%m-%d_%H-%M-%OS}", now);
    std::replace(ts.begin(), ts.end(), ':', '-');

    file_.open(logs_dir / std::format("{}_{}.log", process_name, ts));

    // Start the background consumer thread
    worker_thread_ = std::thread(&Logger::processLogs, this);
}

Logger::~Logger() {
    exit_flag_ = true;
    cv_.notify_all(); // Wake up worker to finish remaining logs
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

void Logger::log_internal(LogLevel level, std::string msg, const std::source_location loc) {
    if (level < min_level_)
        return;

    auto now                 = std::chrono::system_clock::now();
    std::string_view lvl_str = (level == LogLevel::DEBUG)  ? "DEBUG"
                               : (level == LogLevel::INFO) ? "INFO "
                               : (level == LogLevel::WARN) ? "WARN "
                                                           : "ERROR";

    // Format the string on the PRODUCER thread to capture state immediately
    std::string formatted = std::format(
        "{:%H:%M:%OS} | {} | L:{:<4} | {:<20} | {:<15} | {}\n", now, lvl_str, loc.line(),
        loc.function_name(), std::filesystem::path(loc.file_name()).filename().string(), msg);

    // Lock-protected push to the queue
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        log_queue_.push(std::move(formatted));
    }
    cv_.notify_one(); // Signal the background thread
}

void Logger::processLogs() {
    while (true) {
        std::unique_lock<std::mutex> lock(queue_mutex_);

        // Wait until there's a message or we are shutting down
        cv_.wait(lock, [this] { return !log_queue_.empty() || exit_flag_; });

        while (!log_queue_.empty()) {
            std::string line = std::move(log_queue_.front());
            log_queue_.pop();

            // Release lock during I/O to let Producer push more logs
            lock.unlock();
            if (file_.is_open()) {
                file_ << line;
                file_.flush();
            }
            lock.lock();
        }

        cv_.notify_all(); // Notify flush() that queue is empty
        if (exit_flag_)
            break;
    }
}