#include "skybox.h"

#include <GL/glew.h>

#include <stb/stb_image.h>

#include "common/log.h"
#include "common/asserts.h"

LOCAL f32 skybox_vertices[] = {
    -1.0f,  1.0f, -1.0f,
    -1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f, -1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,

    -1.0f, -1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f, -1.0f,  1.0f,
    -1.0f, -1.0f,  1.0f,

    -1.0f,  1.0f, -1.0f,
     1.0f,  1.0f, -1.0f,
     1.0f,  1.0f,  1.0f,
     1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f,  1.0f,
    -1.0f,  1.0f, -1.0f,

    -1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f, -1.0f,
     1.0f, -1.0f, -1.0f,
    -1.0f, -1.0f,  1.0f,
     1.0f, -1.0f,  1.0f
};

bool skybox_create(Skybox_Create_Info *create_info, Skybox *out_skybox)
{
    ASSERT(create_info);
    ASSERT(out_skybox);

    u32 skybox_texture_id;
    glGenTextures(1, &skybox_texture_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_texture_id);

    i32 width, height, nr_channels;

    if (create_info->debug) {
        // Use only one debug texture for all skybox faces.
        // Debug texture file path found at index 0 of face_filepaths.
        u8 *image_data = stbi_load(create_info->face_filepaths[0], &width, &height, &nr_channels, STBI_default);
        if (image_data == NULL) {
            LOG_ERROR("failed to load cubemap face at `%s`\n", create_info->face_filepaths[0]);
            stbi_image_free(image_data);
            return false;
        }

        LOG_DEBUG("loaded debug skybox texture at `%s`\n", create_info->face_filepaths[0]);

        for (u32 i = 0; i < SKYBOX_NUM_FACES; i++) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image_data);
        }

        stbi_image_free(image_data);
    } else {
        for (u32 i = 0; i < SKYBOX_NUM_FACES; i++) {
            u8 *image_data = stbi_load(create_info->face_filepaths[i], &width, &height, &nr_channels, STBI_default);
            LOG_DEBUG("loaded skybox texture at `%s`\n", create_info->face_filepaths[i]);
            if (image_data == NULL) {
                LOG_ERROR("failed to load cubemap face at `%s`\n", create_info->face_filepaths[i]);
                stbi_image_free(image_data);
                return false;
            }

            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image_data);
            stbi_image_free(image_data);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    Shader_Create_Info skybox_create_info = {
        .vertex_filepath = "assets/shaders/skybox.vert",
        .fragment_filepath = "assets/shaders/skybox.frag"
    };

    Shader skybox_shader;
    if (!shader_create(&skybox_create_info, &skybox_shader)) {
        LOG_ERROR("failed to create skybox shader\n");
        return false;
    }

    u32 skybox_vao, skybox_vbo;
    glGenVertexArrays(1, &skybox_vao);
    glBindVertexArray(skybox_vao);

    glGenBuffers(1, &skybox_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, skybox_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skybox_vertices), skybox_vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(f32), (const void *) 0);
    glEnableVertexAttribArray(0);

    out_skybox->shader = skybox_shader;
    out_skybox->vao = skybox_vao;
    out_skybox->vbo = skybox_vbo;
    out_skybox->texture_id = skybox_texture_id;

    return true;
}

void skybox_destroy(Skybox *skybox)
{
    ASSERT(skybox);
    shader_destroy(&skybox->shader);
    glDeleteVertexArrays(1, &skybox->vao);
    glDeleteBuffers(1, &skybox->vbo);
    glDeleteTextures(1, &skybox->texture_id);
}

void skybox_render(Skybox *skybox, const glm::mat4 *projection, const glm::mat4 *view)
{
    glm::mat4 view_no_translate = glm::mat4(glm::mat3(*view));

    glDepthMask(GL_FALSE);
    shader_bind(&skybox->shader);
    shader_set_uniform_int(&skybox->shader, "u_skybox", 0);
    shader_set_uniform_mat4(&skybox->shader, "u_projection", projection);
    shader_set_uniform_mat4(&skybox->shader, "u_view", &view_no_translate);
    glBindVertexArray(skybox->vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox->texture_id);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glDepthMask(GL_TRUE);
}
