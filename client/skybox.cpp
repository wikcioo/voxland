#include "skybox.h"

#include <GL/glew.h>

#include <stb/stb_image.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb/stb_image_resize2.h>
#pragma GCC diagnostic pop

#include "common/log.h"
#include "common/job.h"
#include "common/asserts.h"

#define SKYBOX_DEBUG_TEXTURE_FILEPATH "assets/textures/prototypes/texture_06.png"

typedef struct {
    const char *image_filepath;
    u32 gl_face;
} Skybox_Job_Load_Face_Params;

typedef struct {
    const char *image_filepath;
    u8 *image_data;
    i32 width, height, nr_channels;
    u32 gl_face;
} Skybox_Job_Load_Face_Result;

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

LOCAL const char *gl_face_as_cstr[SKYBOX_NUM_FACES] = {
    "CUBE_MAP_POSITIVE_X",
    "CUBE_MAP_NEGATIVE_X",
    "CUBE_MAP_POSITIVE_Y",
    "CUBE_MAP_NEGATIVE_Y",
    "CUBE_MAP_POSITIVE_Z",
    "CUBE_MAP_NEGATIVE_Z"
};

LOCAL u32 skybox_texture_id;
LOCAL u8 loaded_faces_count = 0;

LOCAL bool skybox_job_load_face_entry_point(void *param_data, void *result_data)
{
    Skybox_Job_Load_Face_Params *params = (Skybox_Job_Load_Face_Params *) param_data;

    i32 width, height, nr_channels;
    u8 *image_data = stbi_load(params->image_filepath, &width, &height, &nr_channels, STBI_default);
    if (image_data == NULL) {
        LOG_ERROR("failed to load cubemap face at `%s`\n", params->image_filepath);
        stbi_image_free(image_data);
        return false;
    }

    Skybox_Job_Load_Face_Result *results = (Skybox_Job_Load_Face_Result *) result_data;
    results->image_filepath = params->image_filepath,
    results->image_data = image_data;
    results->width = width;
    results->height = height;
    results->nr_channels = nr_channels;
    results->gl_face = params->gl_face;

    return true;
}

LOCAL void skybox_job_load_face_callback(Job_Status status, void *result_data)
{
    if (status == JOB_STATUS_FAILED) {
        return;
    }

    Skybox_Job_Load_Face_Result *results = (Skybox_Job_Load_Face_Result *) result_data;

    if (results->image_data == NULL) {
        LOG_ERROR("image data not initialized\n");
        return;
    }

    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_texture_id);
    glTexSubImage2D(results->gl_face, 0, 0, 0, results->width, results->height, GL_RGB, GL_UNSIGNED_BYTE, results->image_data);
    stbi_image_free(results->image_data);

    const char *face_as_cstr = gl_face_as_cstr[results->gl_face - GL_TEXTURE_CUBE_MAP_POSITIVE_X];
    LOG_TRACE("skybox loaded %s face from `%s`\n", face_as_cstr, results->image_filepath);

    loaded_faces_count += 1;
    if (loaded_faces_count == SKYBOX_NUM_FACES) {
        LOG_TRACE("loaded all skybox faces\n");
    }
}

LOCAL void skybox_init_jobs(Skybox_Create_Info *create_info)
{
    Skybox_Job_Load_Face_Params params = {};
    for (u32 i = 0; i < SKYBOX_NUM_FACES; i++) {
        params.image_filepath = create_info->face_filepaths[i];
        params.gl_face = GL_TEXTURE_CUBE_MAP_POSITIVE_X + i;
        job_system_submit(skybox_job_load_face_entry_point, &params, sizeof(Skybox_Job_Load_Face_Params), skybox_job_load_face_callback, sizeof(Skybox_Job_Load_Face_Result));
    }
}

bool skybox_create(Skybox_Create_Info *create_info, Skybox *out_skybox)
{
    ASSERT(create_info);
    ASSERT(out_skybox);

    glGenTextures(1, &skybox_texture_id);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox_texture_id);

    i32 width, height, nr_channels;
    u8 *debug_image_data = stbi_load(SKYBOX_DEBUG_TEXTURE_FILEPATH, &width, &height, &nr_channels, STBI_default);

    if (!create_info->debug_only) {
        i32 w, h, c;
        if (!stbi_info(create_info->face_filepaths[0], &w, &h, &c)) {
            LOG_ERROR("failed to read image dimensions for `%s`\n", create_info->face_filepaths[0]);
            return false;
        }

        u8 *debug_image_data_resized = stbir_resize_uint8_linear(debug_image_data, width, height, 0, 0, w, h, 0, (stbir_pixel_layout) nr_channels);
        if (debug_image_data_resized == NULL) {
            LOG_ERROR("failed to resize debug image data\n");
            stbi_image_free(debug_image_data);
            return false;
        }

        width = w;
        height = h;
        stbi_image_free(debug_image_data);
        debug_image_data = debug_image_data_resized;
    }

    for (u32 i = 0; i < SKYBOX_NUM_FACES; i++) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, debug_image_data);
    }

    if (!create_info->debug_only) {
        free(debug_image_data);
    } else {
        stbi_image_free(debug_image_data);
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    if (!create_info->debug_only) {
        skybox_init_jobs(create_info);
    }

    Shader_Create_Info skybox_create_info = {
        .vertex_filepath = "assets/shaders/skybox.vert",
        .geometry_filepath = NULL,
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
