#pragma once

#include "defines.h"

#if defined(__GNUC__) && defined(__cplusplus)
    #define DEBUG_BREAK() __builtin_trap()
#else
    #error "Only g++ compiler is supported!"
#endif

void report_assertion_failure(const char *expression, const char *message, const char *file, i32 line);

#ifndef ENABLE_ASSERTIONS
    #define ENABLE_ASSERTIONS 1
#endif

#if ENABLE_ASSERTIONS
    #define ASSERT(expr)                                                \
        {                                                               \
            if (!(expr)) {                                              \
                report_assertion_failure(#expr, 0, __FILE__, __LINE__); \
                DEBUG_BREAK();                                          \
            }                                                           \
        }

    #define ASSERT_MSG(expr, message)                                         \
        {                                                                     \
            if (!(expr)) {                                                    \
                report_assertion_failure(#expr, message, __FILE__, __LINE__); \
                DEBUG_BREAK();                                                \
            }                                                                 \
        }
#else
    #define ASSERT(expr)
    #define ASSERT_MSG(expr, message)
#endif
