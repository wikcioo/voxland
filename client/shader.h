#pragma once

#include <glm/glm.hpp>

#include "common/defines.h"

typedef struct {
    u32 program;
} Shader;

typedef struct {
    const char *vertex_filepath;
    const char *fragment_filepath;
} Shader_Create_Info;

bool shader_create(const Shader_Create_Info *create_info, Shader *out_shader);
void shader_destroy(Shader *shader);

void shader_bind(Shader *shader);
void shader_unbind(Shader *shader);

void shader_set_uniform_int(Shader *shader, const char *name, i32 data);
void shader_set_uniform_float(Shader *shader, const char *name, f32 data);
void shader_set_uniform_int_array(Shader *shader, const char *name, const i32 *data, u32 length);
void shader_set_uniform_vec2(Shader *shader, const char *name, const glm::vec2 *data);
void shader_set_uniform_vec3(Shader *shader, const char *name, const glm::vec3 *data);
void shader_set_uniform_vec4(Shader *shader, const char *name, const glm::vec4 *data);
void shader_set_uniform_mat4(Shader *shader, const char *name, const glm::mat4 *data);
