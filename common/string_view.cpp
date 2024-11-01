#include "string_view.h"

#include <ctype.h>
#include <string.h>

String_View sv_from_cstr(const char *cstr)
{
    return String_View {
        .count = (u32) strlen(cstr),
        .data = cstr
    };
}

String_View sv_trim_left(String_View sv)
{
    u32 i = 0;
    while (i < sv.count && isspace(sv.data[i])) {
        i++;
    }

    return String_View {
        .count = sv.count - i,
        .data = sv.data + i
    };
}

String_View sv_trim_right(String_View sv)
{
    u32 i = 0;
    while (i < sv.count && isspace(sv.data[sv.count - 1 - i])) {
        i++;
    }

    return String_View {
        .count = sv.count - i,
        .data = sv.data
    };
}

String_View sv_trim(String_View sv)
{
    return sv_trim_right(sv_trim_left(sv));
}

String_View sv_chop_by_delim(String_View *sv, char delim)
{
    u32 i = 0;
    while (i < sv->count && sv->data[i] != delim) {
        i++;
    }

    String_View result = String_View {
        .count = i,
        .data = sv->data
    };

    if (i < sv->count) {
        sv->count -= i + 1;
        sv->data += i + 1;
    } else {
        sv->count -= i;
        sv->data += i;
    }

    return result;
}

bool sv_eq(String_View a, String_View b)
{
    if (a.count != b.count) {
        return 0;
    }

    return memcmp(a.data, b.data, a.count) == 0;
}

i32 sv_to_i32(String_View sv)
{
    i32 result = 0;

    for (u32 i = 0; i < sv.count && isdigit(sv.data[i]); i++) {
        result = result * 10 + (sv.data[i] - '0');
    }

    return result;
}
