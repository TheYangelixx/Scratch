#ifndef MYSCRATCH_LOGGER_H
#define MYSCRATCH_LOGGER_H

#include <string>

enum LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR
};

void log_init(const std::string& log_file = "");
void log_shutdown();

void log_message(LogLevel level, const std::string& msg);

void log_debug(const std::string& msg);
void log_info(const std::string& msg);
void log_warn(const std::string& msg);
void log_error(const std::string& msg);

#endif // MYSCRATCH_LOGGER_H
