#pragma once

#include "common/defines.h"

typedef enum {
    IMAGE_FORMAT_NONE,
    IMAGE_FORMAT_R8,
    IMAGE_FORMAT_RGB8,
    IMAGE_FORMAT_RGBA8,
    IMAGE_FORMAT_COUNT
} Image_Format;

typedef struct {
    u32 width;
    u32 height;
    bool generate_mipmaps;
    Image_Format format;
} Texture_Spec;

typedef struct {
    u32 id;
    u32 width;
    u32 height;
    u32 format;
} Texture;

void texture_create_from_path(const char *filepath, Texture *out_texture);
void texture_create_from_spec(Texture_Spec spec, const void *data, Texture *out_texture);
void texture_set_data(Texture *texture, const void *data);
void texture_destroy(Texture *texture);
