#include "log.h"

#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <string.h>

#include "defines.h"
#include "event.h"
#include "collections/darray.h"

LOCAL Log_Registry *log_registry;

LOCAL const char *levels_str[]   = { "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL" };
LOCAL const char *levels_color[] = {  "1;37",  "1;34", "1;32", "1;33",  "1;31",  "1;41" };

time_t get_current_time_as_cstr(char *buf, u32 buf_len)
{
    time_t raw;
    struct tm *local_time;
    time(&raw);
    local_time = localtime(&raw);
    strftime(buf, buf_len, "%H:%M:%S", local_time);
    return raw;
}

void log_init(Log_Registry *lr)
{
    log_registry = lr;
}

void log_message(Log_Level level, const char *message, ...)
{
    FILE *stream = level > LOG_LEVEL_WARN ? stderr : stdout;

    char current_time[16] = {0};
    time_t timestamp = get_current_time_as_cstr(current_time, 16);

    va_list args;
    va_start(args, message);
    char formatted_message_buffer[512] = {};
    vsnprintf(formatted_message_buffer, sizeof(formatted_message_buffer), message, args);
    va_end(args);

    char full_message_buffer[1024] = {};
    i32 bytes_written = snprintf(full_message_buffer, sizeof(full_message_buffer), "[%s] \033[%sm[%-5s]\033[0m ", current_time, levels_color[level], levels_str[level]);
    snprintf(full_message_buffer + bytes_written, sizeof(full_message_buffer) - bytes_written, "%s", formatted_message_buffer);

    // Write to stdout/stderr
    fprintf(stream, "%s", full_message_buffer);

    // Append new log entry to the global log registry and generate event
    if (log_registry == NULL) {
        fprintf(stderr, "log_message: log_registry not initialized\n");
        return;
    }

    if (!arena_allocator_can_allocate(&log_registry->allocator, strlen(formatted_message_buffer)+1)) {
        fprintf(stderr, ERROR_COLOR "ran out of free space for logs in the registry\n" RESET_COLOR);
        return;
    }

    char *log_message = (char *) arena_allocator_allocate(&log_registry->allocator, strlen(formatted_message_buffer)+1);
    memcpy(log_message, formatted_message_buffer, strlen(formatted_message_buffer));

    Log_Entry log = {
        .timestamp = timestamp,
        .level = level,
        .content = log_message
    };

    darray_push(log_registry->logs, log);
    event_system_fire(EVENT_CODE_APP_LOG, {0});
}

void report_assertion_failure(const char *expression, const char *message, const char *file, i32 line, ...)
{
    bool with_msg = message != NULL;

    char formatted_message[256] = {};
    if (with_msg) {
        va_list args;
        va_start(args, line);
        vsnprintf(formatted_message, sizeof(formatted_message), message, args);
        va_end(args);
    }

    char formatted_report[512] = {};
    snprintf(formatted_report, sizeof(formatted_report),
             "assertion failure for expression `%s`%s%s%s in %s:%d\n",
             expression,
             with_msg ? " with message \"" : "",
             with_msg ? formatted_message : "",
             with_msg ? "\"" : "",
             file, line);

#if ENABLE_FATAL_LOG
    LOG_FATAL("%s", formatted_report);
#else
    fprintf(stderr, "%s", formatted_report);
#endif
}

void report_expect_failure(const char *message, ...)
{
    va_list args;
    va_start(args, message);
    fprintf(stderr, ERROR_COLOR "failed: ");
    vfprintf(stderr, message, args);
    fprintf(stderr, RESET_COLOR);
    va_end(args);
}
