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

void log_message(log_level_e level, const char *message, ...)
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
