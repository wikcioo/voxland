#include "hexdump.h"

void hexdump(FILE *stream, u8 *buffer, u64 buffer_size, i32 flags)
{
    bool canonical = flags & HEXDUMP_FLAG_CANONICAL;

    u64 rows = buffer_size / HEXDUMP_MAX_BYTES_PER_ROW;
    for (u64 i = 0; i < rows; i++) {
        fprintf(stream, "%08llX  ", i * HEXDUMP_MAX_BYTES_PER_ROW);
        for (u32 j = 0; j < HEXDUMP_MAX_BYTES_PER_ROW; j++) {
            fprintf(stream, "%02X ", buffer[i * HEXDUMP_MAX_BYTES_PER_ROW + j]);
        }

        if (canonical) {
            fprintf(stream, " |");

            for (u32 j = 0; j < HEXDUMP_MAX_BYTES_PER_ROW; j++) {
                u8 c = buffer[i * HEXDUMP_MAX_BYTES_PER_ROW + j];
                fprintf(stream, "%c", c >= 32 && c < 128 ? (char) c : '.');
            }
            fprintf(stream, "|\n");
        } else {
            fprintf(stream, "\n");
        }
    }

    u32 remaining_bytes = (u32) (buffer_size % HEXDUMP_MAX_BYTES_PER_ROW);
    if (remaining_bytes > 0) {
        fprintf(stream, "%08llX  ", rows * HEXDUMP_MAX_BYTES_PER_ROW);
        for (u32 j = 0; j < remaining_bytes; j++) {
            fprintf(stream, "%02X ", buffer[rows * HEXDUMP_MAX_BYTES_PER_ROW + j]);
        }

        for (u32 j = 0; j < HEXDUMP_MAX_BYTES_PER_ROW - remaining_bytes; j++) {
            fprintf(stream, "   ");
        }

        if (canonical) {
            fprintf(stream, " |");

            for (u32 j = 0; j < remaining_bytes; j++) {
                u8 c = buffer[rows * HEXDUMP_MAX_BYTES_PER_ROW + j];
                fprintf(stream, "%c", c >= 32 && c < 128 ? (char) c : '.');
            }
            fprintf(stream, "|\n");
        } else {
            fprintf(stream, "\n");
        }
    }
}
