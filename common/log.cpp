#include "log.h"

#include <stdio.h>
#include <stdarg.h>
#include <time.h>

#include "defines.h"

LOCAL const char *levels_str[]   = { "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL" };
LOCAL const char *levels_color[] = {  "1;37",  "1;34", "1;32", "1;33",  "1;31",  "1;41" };

void get_current_time_as_cstr(char *buf, u32 buf_len)
{
    time_t raw;
    struct tm *local_time;
    time(&raw);
    local_time = localtime(&raw);
    strftime(buf, buf_len, "%H:%M:%S", local_time);
}

void log_message(Log_Level level, const char *message, ...)
{
    FILE *stream = level > LOG_LEVEL_WARN ? stderr : stdout;

    char current_time[16] = {0};
    get_current_time_as_cstr(current_time, 16);

    va_list args;
    va_start(args, message);
    fprintf(stream, "[%s] \033[%sm[%-5s]\033[0m ", current_time, levels_color[level], levels_str[level]);
    vfprintf(stream, message, args);
    va_end(args);
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
