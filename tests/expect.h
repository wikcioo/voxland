#pragma once

#include <stdio.h>
#include <stdarg.h>

void report_expect_failure(const char *message, ...);

#define expect_equal(actual, expected)                                                                            \
    if (actual != expected) {                                                                                     \
        report_expect_failure("expected `%lld` but got `%lld` in %s:%d\n", expected, actual, __FILE__, __LINE__); \
        return false;                                                                                             \
    }

#define expect_not_equal(actual, expected)                                                                                   \
    if (actual == expected) {                                                                                                \
        report_expect_failure("expected `%lld` to not be equal to `%lld` in %s:%d\n", expected, actual, __FILE__, __LINE__); \
        return false;                                                                                                        \
    }

#define expect_true(condition)                                                                                  \
    if (!(condition)) {                                                                                         \
        report_expect_failure("expected condition `%s` to be true in %s:%d\n", #condition, __FILE__, __LINE__); \
        return false;                                                                                           \
    }

#define expect_false(condition)                                                                                  \
    if (condition) {                                                                                             \
        report_expect_failure("expected condition `%s` to be false in %s:%d\n", #condition, __FILE__, __LINE__); \
        return false;                                                                                            \
    }
