#pragma once

#include <time.h>

#include "memory/arena_allocator.h"

#ifndef ENABLE_LOGGING
    #define ENABLE_LOGGING 1
#endif

#if ENABLE_LOGGING
    #ifndef ENABLE_TRACE_LOG
        #define ENABLE_TRACE_LOG 1
    #endif
    #ifndef ENABLE_DEBUG_LOG
        #define ENABLE_DEBUG_LOG 1
    #endif
    #ifndef ENABLE_INFO_LOG
        #define ENABLE_INFO_LOG 1
    #endif
    #ifndef ENABLE_WARN_LOG
        #define ENABLE_WARN_LOG 1
    #endif
    #ifndef ENABLE_ERROR_LOG
        #define ENABLE_ERROR_LOG 1
    #endif
    #ifndef ENABLE_FATAL_LOG
        #define ENABLE_FATAL_LOG 1
    #endif
#else
    #define ENABLE_TRACE_LOG 0
    #define ENABLE_DEBUG_LOG 0
    #define ENABLE_INFO_LOG  0
    #define ENABLE_WARN_LOG  0
    #define ENABLE_ERROR_LOG 0
    #define ENABLE_FATAL_LOG 0
#endif

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
} Log_Level;

typedef struct {
    time_t timestamp;
    Log_Level level;
    const char *content;
} Log_Entry;

typedef struct {
    Arena_Allocator allocator;
    Log_Entry *logs; // darray
} Log_Registry;

extern Log_Registry log_registry;

void log_message(Log_Level level, const char *message, ...);

#if ENABLE_TRACE_LOG
    #define LOG_TRACE(message, ...) log_message(LOG_LEVEL_TRACE, message, ##__VA_ARGS__)
#else
    #define LOG_TRACE(message, ...)
#endif

#if ENABLE_DEBUG_LOG
    #define LOG_DEBUG(message, ...) log_message(LOG_LEVEL_DEBUG, message, ##__VA_ARGS__)
#else
    #define LOG_DEBUG(message, ...)
#endif

#if ENABLE_INFO_LOG
    #define LOG_INFO(message, ...) log_message(LOG_LEVEL_INFO,  message, ##__VA_ARGS__)
#else
    #define LOG_INFO(message, ...)
#endif

#if ENABLE_WARN_LOG
    #define LOG_WARN(message, ...) log_message(LOG_LEVEL_WARN,  message, ##__VA_ARGS__)
#else
    #define LOG_WARN(message, ...)
#endif

#if ENABLE_ERROR_LOG
    #define LOG_ERROR(message, ...) log_message(LOG_LEVEL_ERROR, message, ##__VA_ARGS__)
#else
    #define LOG_ERROR(message, ...)
#endif

#if ENABLE_FATAL_LOG
    #define LOG_FATAL(message, ...) log_message(LOG_LEVEL_FATAL, message, ##__VA_ARGS__)
#else
    #define LOG_FATAL(message, ...)
#endif
