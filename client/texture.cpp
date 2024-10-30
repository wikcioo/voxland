#include "texture.h"

#include <memory.h>

#include <GL/glew.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#pragma GCC diagnostic pop

#include "common/log.h"
#include "common/asserts.h"
#include "common/filesystem.h"

static u32 image_format_to_opengl_format(Image_Format format)
{
    ASSERT(format > IMAGE_FORMAT_NONE && format < IMAGE_FORMAT_COUNT);

    switch (format) {
        case IMAGE_FORMAT_R8:    return GL_RED;
        case IMAGE_FORMAT_RGB8:  return GL_RGB;
        case IMAGE_FORMAT_RGBA8: return GL_RGBA;
        case IMAGE_FORMAT_NONE:
        case IMAGE_FORMAT_COUNT:
        default:
            LOG_WARN("unknown image format `0x%X`\n", format);
            return 0;
    }
}

void texture_create_from_path(const char *filepath, Texture *out_texture)
{
    ASSERT(filepath);
    ASSERT(out_texture);
    ASSERT(filesystem_exists(filepath));

    i32 width, height, channels;
    stbi_set_flip_vertically_on_load(true);
    u8 *image_data = stbi_load(filepath, &width, &height, &channels, STBI_default);
    if (image_data == NULL) {
        LOG_ERROR("failed to load image at `%s`\n", filepath);
        return;
    }

    memset(out_texture, 0, sizeof(Texture));
    out_texture->width = width;
    out_texture->height = height;

    u32 format = 0;
    if (channels == 1) {
        format = GL_RED;
    } else if (channels == 3) {
        format = GL_RGB;
    } else if (channels == 4) {
        format = GL_RGBA;
    } else {
        LOG_WARN("unsupported number of channels `%d` for image at `%s`\n", channels, filepath);
        return;
    }

    glGenTextures(1, &out_texture->id);
    glBindTexture(GL_TEXTURE_2D, out_texture->id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, image_data);

    stbi_image_free(image_data);
}

void texture_create_from_spec(Texture_Spec spec, const void *data, Texture *out_texture)
{
    ASSERT(out_texture);

    u32 format = image_format_to_opengl_format(spec.format);

    memset(out_texture, 0, sizeof(Texture));
    out_texture->width = spec.width;
    out_texture->height = spec.height;
    out_texture->format = format;

    glGenTextures(1, &out_texture->id);
    glBindTexture(GL_TEXTURE_2D, out_texture->id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, format, spec.width, spec.height, 0, format, GL_UNSIGNED_BYTE, data);

    if (spec.generate_mipmaps) {
        glGenerateMipmap(GL_TEXTURE_2D);
    }
}

void texture_set_data(Texture *texture, const void *data)
{
    glBindTexture(GL_TEXTURE_2D, texture->id);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texture->width, texture->height, texture->format, GL_UNSIGNED_BYTE, data);
}

void texture_destroy(Texture *texture)
{
    ASSERT(texture);
    glDeleteTextures(1, &texture->id);
}
