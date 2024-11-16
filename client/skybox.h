#pragma once

#include <glm/glm.hpp>

#include "shader.h"
#include "common/defines.h"

#define SKYBOX_NUM_FACES 6

typedef struct {
    bool debug_only;
    const char *face_filepaths[SKYBOX_NUM_FACES];
} Skybox_Create_Info;

typedef struct {
    Shader shader;
    u32 vao, vbo;
    u32 texture_id;
} Skybox;

bool skybox_create(Skybox_Create_Info *create_info, Skybox *out_skybox);
void skybox_destroy(Skybox *skybox);
void skybox_render(Skybox *skybox, const glm::mat4 *projection, const glm::mat4 *view);
