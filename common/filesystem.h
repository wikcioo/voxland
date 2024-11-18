#pragma once

#include "defines.h"

typedef struct {
    void *handle;
    u32 modes;
    bool is_valid  : 1;
    bool is_binary : 1;
} File_Handle;

typedef enum {
    FILE_MODE_READ   = BIT(0),
    FILE_MODE_WRITE  = BIT(1),
    FILE_MODE_APPEND = BIT(2)
} File_Mode;

bool filesystem_open(const char *path, u32 mode, bool is_binary, File_Handle *out_handle);
void filesystem_close(File_Handle *handle);

bool filesystem_exists(const char *path);
void filesystem_get_size(const File_Handle *handle, u64 *out_size);

bool filesystem_read_all(const File_Handle *handle, void *buffer, u64 *out_bytes_read);
bool filesystem_write(const File_Handle *handle, void *buffer, u64 size);
