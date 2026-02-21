#include "logger.h"
#include <iostream>
#include <fstream>
#include <ctime>
#include <mutex>

static std::ofstream g_log_file;
static std::mutex    g_log_mutex;
static bool          g_initialized = false;

static std::string level_to_string(LogLevel level) {
    switch (level) {
        case LOG_DEBUG: return "DEBUG";
        case LOG_INFO:  return "INFO ";
        case LOG_WARN:  return "WARN ";
        case LOG_ERROR: return "ERROR";
        default:        return "?????";
    }
}

static std::string current_timestamp() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", t);
    return std::string(buf);
}

void log_init(const std::string& log_file) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (!log_file.empty()) {
        g_log_file.open(log_file, std::ios::out | std::ios::app);
    }
    g_initialized = true;
}

void log_shutdown() {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    if (g_log_file.is_open()) {
        g_log_file.close();
    }
    g_initialized = false;
}

void log_message(LogLevel level, const std::string& msg) {
    std::lock_guard<std::mutex> lock(g_log_mutex);
    std::string line = "[" + current_timestamp() + "] [" + level_to_string(level) + "] " + msg;
    std::cout << line << std::endl;
    if (g_log_file.is_open()) {
        g_log_file << line << std::endl;
    }
}

void log_debug(const std::string& msg) { log_message(LOG_DEBUG, msg); }
void log_info(const std::string& msg)  { log_message(LOG_INFO,  msg); }
void log_warn(const std::string& msg)  { log_message(LOG_WARN,  msg); }
void log_error(const std::string& msg) { log_message(LOG_ERROR, msg); }
