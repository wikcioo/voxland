#pragma once

#include "defines.h"

typedef struct {
    u32 count;
    const char *data;
} String_View;

#define SV_FMT(sv) (i32) sv.count, sv.data

String_View sv_from_cstr(const char *cstr);
String_View sv_trim_left(String_View sv);
String_View sv_trim_right(String_View sv);
String_View sv_trim(String_View sv);
String_View sv_chop_by_delim(String_View *sv, char delim);
bool sv_eq(String_View a, String_View b);
i32 sv_to_int(String_View sv);
