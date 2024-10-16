#pragma once

#include <stdio.h>

#include "defines.h"

#define HEXDUMP_MAX_BYTES_PER_ROW 16
#define HEXDUMP_FLAG_CANONICAL BIT(0)

void hexdump(FILE *stream, u8 *buffer, u64 buffer_size, i32 flags);
