#pragma once

#define ENABLE_TRACE_LOG 1

#if defined(DEBUG)
    #define ENABLE_DEBUG_LOG 1
#endif

#define ENABLE_INFO_LOG 1
#define ENABLE_WARN_LOG 1

#define RESET_COLOR "\033[0m"
#define TRACE_COLOR "\033[1;37m"
#define DEBUG_COLOR "\033[34m"
#define INFO_COLOR  "\033[32m"
#define WARN_COLOR  "\033[1;33m"
#define ERROR_COLOR "\033[1;31m"
#define FATAL_COLOR "\033[1;41m"

typedef enum {
    LOG_LEVEL_TRACE,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_FATAL
} log_level_e;

void log_message(log_level_e level, const char *message, ...);

#if defined(ENABLE_TRACE_LOG)
    #define LOG_TRACE(message, ...) log_message(LOG_LEVEL_TRACE, message, ##__VA_ARGS__)
#else
    #define LOG_TRACE(message, ...)
#endif

#if defined(ENABLE_DEBUG_LOG)
    #define LOG_DEBUG(message, ...) log_message(LOG_LEVEL_DEBUG, message, ##__VA_ARGS__)
#else
    #define LOG_DEBUG(message, ...)
#endif

#if defined(ENABLE_INFO_LOG)
    #define LOG_INFO(message, ...) log_message(LOG_LEVEL_INFO,  message, ##__VA_ARGS__)
#else
    #define LOG_INFO(message, ...)
#endif

#if defined(ENABLE_WARN_LOG)
    #define LOG_WARN(message, ...) log_message(LOG_LEVEL_WARN,  message, ##__VA_ARGS__)
#else
    #define LOG_WARN(message, ...)
#endif

#define LOG_ERROR(message, ...) log_message(LOG_LEVEL_ERROR, message, ##__VA_ARGS__)
#define LOG_FATAL(message, ...) log_message(LOG_LEVEL_FATAL, message, ##__VA_ARGS__)