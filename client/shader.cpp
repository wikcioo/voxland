#include "shader.h"

#include <stdio.h>
#include <stdlib.h>

#include <GL/glew.h>

#include "common/log.h"
#include "common/asserts.h"
#include "common/filesystem.h"
#include "common/memory/memutils.h"

bool shader_create(const Shader_Create_Info *create_info, Shader *out_shader)
{
    ASSERT(create_info)
    ASSERT(out_shader);
    ASSERT(create_info->vertex_filepath);
    ASSERT(create_info->fragment_filepath);

    bool has_geometry_shader = create_info->geometry_filepath != NULL;

    if (!filesystem_exists(create_info->vertex_filepath)) {
        LOG_ERROR("vertex shader source at `%s` not found\n", create_info->vertex_filepath);
        return false;
    }

    if (has_geometry_shader && !filesystem_exists(create_info->geometry_filepath)) {
        LOG_ERROR("geometry shader source at `%s` not found\n", create_info->geometry_filepath);
        return false;
    }

    if (!filesystem_exists(create_info->fragment_filepath)) {
        LOG_ERROR("fragment shader source at `%s` not found\n", create_info->vertex_filepath);
        return false;
    }

    File_Handle vertex_file_handle;
    File_Handle geometry_file_handle;
    File_Handle fragment_file_handle;
    filesystem_open(create_info->vertex_filepath, FILE_MODE_READ, false, &vertex_file_handle);
    if (has_geometry_shader) {
        filesystem_open(create_info->geometry_filepath, FILE_MODE_READ, false, &geometry_file_handle);
    }
    filesystem_open(create_info->fragment_filepath, FILE_MODE_READ, false, &fragment_file_handle);

    u64 vertex_file_size = 0, geometry_file_size = 0, fragment_file_size = 0;
    filesystem_get_size(&vertex_file_handle, &vertex_file_size);
    if (has_geometry_shader) {
        filesystem_get_size(&geometry_file_handle, &geometry_file_size);
    }
    filesystem_get_size(&fragment_file_handle, &fragment_file_size);

    // TODO: Replace with arena allocator
    char *vertex_source_buffer = (char *) mem_alloc(vertex_file_size+1, MEMORY_TAG_OPENGL);
    char *geometry_source_buffer = NULL;
    if (has_geometry_shader) {
        geometry_source_buffer = (char *) mem_alloc(geometry_file_size+1, MEMORY_TAG_OPENGL);
    }
    char *fragment_source_buffer = (char *) mem_alloc(fragment_file_size+1, MEMORY_TAG_OPENGL);

    if (!filesystem_read_all(&vertex_file_handle, vertex_source_buffer, NULL)) {
        LOG_ERROR("failed to read all bytes from file at `%s`\n", create_info->vertex_filepath);
    }
    if (has_geometry_shader) {
        if (!filesystem_read_all(&geometry_file_handle, geometry_source_buffer, NULL)) {
            LOG_ERROR("failed to read all bytes from file at `%s`\n", create_info->geometry_filepath);
        }
    }
    if (!filesystem_read_all(&fragment_file_handle, fragment_source_buffer, NULL)) {
        LOG_ERROR("failed to read all bytes from file at `%s`\n", create_info->fragment_filepath);
    }

    vertex_source_buffer[vertex_file_size] = '\0';
    if (has_geometry_shader) {
        geometry_source_buffer[geometry_file_size] = '\0';
    }
    fragment_source_buffer[fragment_file_size] = '\0';

    filesystem_close(&vertex_file_handle);
    if (has_geometry_shader) {
        filesystem_close(&geometry_file_handle);
    }
    filesystem_close(&fragment_file_handle);

    u32 vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex_shader, 1, (const char **) &vertex_source_buffer, NULL);
    glCompileShader(vertex_shader);

    i32 is_compiled;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &is_compiled);
    if (is_compiled == GL_FALSE)
    {
        char info_log[1024];
        glGetShaderInfoLog(vertex_shader, 1024, 0, info_log);
        LOG_ERROR("failed to compile vertex shader: %s", info_log);
    }

    u32 geometry_shader = 0;
    if (has_geometry_shader) {
        geometry_shader = glCreateShader(GL_GEOMETRY_SHADER);
        glShaderSource(geometry_shader, 1, (const char **) &geometry_source_buffer, NULL);
        glCompileShader(geometry_shader);

        glGetShaderiv(geometry_shader, GL_COMPILE_STATUS, &is_compiled);
        if (is_compiled == GL_FALSE)
        {
            char info_log[1024];
            glGetShaderInfoLog(geometry_shader, 1024, 0, info_log);
            LOG_ERROR("failed to compile geometry shader: %s", info_log);
        }
    }

    u32 fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment_shader, 1, (const char **) &fragment_source_buffer, NULL);
    glCompileShader(fragment_shader);

    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &is_compiled);
    if (is_compiled == GL_FALSE)
    {
        char info_log[1024];
        glGetShaderInfoLog(fragment_shader, 1024, 0, info_log);
        LOG_ERROR("failed to compile fragment shader: %s", info_log);
    }

    mem_free(vertex_source_buffer, vertex_file_size+1, MEMORY_TAG_OPENGL);
    if (has_geometry_shader) {
        mem_free(geometry_source_buffer, geometry_file_size+1, MEMORY_TAG_OPENGL);
    }
    mem_free(fragment_source_buffer, fragment_file_size+1, MEMORY_TAG_OPENGL);

    u32 program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    if (has_geometry_shader) {
        glAttachShader(program, geometry_shader);
    }
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    i32 is_linked;
    glGetProgramiv(program, GL_LINK_STATUS, &is_linked);
    if (!is_linked) {
        LOG_ERROR("failed to link the program\n");
        return false;
    }

    glDeleteShader(vertex_shader);
    if (has_geometry_shader) {
        glDeleteShader(geometry_shader);
    }
    glDeleteShader(fragment_shader);

    out_shader->program = program;

    return true;
}

void shader_destroy(Shader *shader)
{
    ASSERT(shader);
    glDeleteProgram(shader->program);
}

void shader_bind(Shader *shader)
{
    ASSERT(shader);
    glUseProgram(shader->program);
}

void shader_unbind(Shader *shader)
{
    ASSERT(shader); UNUSED(shader);
    glUseProgram(0);
}

void shader_set_uniform_int(Shader *shader, const char *name, i32 data)
{
    ASSERT(shader);
    ASSERT(name);

    i32 location = glGetUniformLocation(shader->program, name);
    ASSERT_MSG(location != -1, "failed to find uniform `%s` inside shader program `%u`", name, shader->program);
    glUniform1i(location, data);
}

void shader_set_uniform_float(Shader *shader, const char *name, f32 data)
{
    ASSERT(shader);
    ASSERT(name);

    i32 location = glGetUniformLocation(shader->program, name);
    ASSERT_MSG(location != -1, "failed to find uniform `%s` inside shader program `%u`", name, shader->program);
    glUniform1f(location, data);
}

void shader_set_uniform_int_array(Shader *shader, const char *name, const i32 *data, u32 length)
{
    ASSERT(shader);
    ASSERT(name);
    ASSERT(data);
    ASSERT(length > 0);

    i32 location = glGetUniformLocation(shader->program, name);
    ASSERT_MSG(location != -1, "failed to find uniform `%s` inside shader program `%u`", name, shader->program);
    glUniform1iv(location, length, data);
}

void shader_set_uniform_vec2(Shader *shader, const char *name, const glm::vec2 *data)
{
    ASSERT(shader);
    ASSERT(name);
    ASSERT(data);

    i32 location = glGetUniformLocation(shader->program, name);
    ASSERT_MSG(location != -1, "failed to find uniform `%s` inside shader program `%u`", name, shader->program);
    glUniform2fv(location, 1, (f32 *) data);
}

void shader_set_uniform_vec3(Shader *shader, const char *name, const glm::vec3 *data)
{
    ASSERT(shader);
    ASSERT(name);
    ASSERT(data);

    i32 location = glGetUniformLocation(shader->program, name);
    ASSERT_MSG(location != -1, "failed to find uniform `%s` inside shader program `%u`", name, shader->program);
    glUniform3fv(location, 1, (f32 *) data);
}

void shader_set_uniform_vec4(Shader *shader, const char *name, const glm::vec4 *data)
{
    ASSERT(shader);
    ASSERT(name);
    ASSERT(data);

    i32 location = glGetUniformLocation(shader->program, name);
    ASSERT_MSG(location != -1, "failed to find uniform `%s` inside shader program `%u`", name, shader->program);
    glUniform4fv(location, 1, (f32 *) data);
}

void shader_set_uniform_mat4(Shader *shader, const char *name, const glm::mat4 *data)
{
    ASSERT(shader);
    ASSERT(name);
    ASSERT(data);

    i32 location = glGetUniformLocation(shader->program, name);
    ASSERT_MSG(location != -1, "failed to find uniform `%s` inside shader program `%u`", name, shader->program);
    glUniformMatrix4fv(location, 1, false, (f32 *) data);
}

void shader_set_uniform_mat4_array(Shader *shader, const char *name, const glm::mat4 *data, u32 length)
{
    ASSERT(shader);
    ASSERT(name);
    ASSERT(data);

    i32 location = glGetUniformLocation(shader->program, name);
    ASSERT_MSG(location != -1, "failed to find uniform `%s` inside shader program `%u`", name, shader->program);
    glUniformMatrix4fv(location, length, false, (f32 *) data);
}
