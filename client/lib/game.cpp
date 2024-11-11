#include "game.h"

#include <stdio.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "client/global.h"
#include "common/log.h"
#include "common/packet.h"
#include "common/clock.h"
#include "common/asserts.h"

const i32 num_instances = 100 * 100;

void process_input(Game *game, f32 dt)
{
    glm::vec3 camera_right = glm::normalize(glm::cross(game->global_data->camera_direction, game->global_data->camera_up));

    PERSIST f32 camera_velocity = 5.0f;
    f32 camera_speed = dt * camera_velocity * game->global_data->camera_speed_factor;

    if (game->global_data->keys_state[KEYCODE_W]) {
        game->global_data->camera_position += game->global_data->camera_direction * camera_speed;
    } else if (game->global_data->keys_state[KEYCODE_S]) {
        game->global_data->camera_position -= game->global_data->camera_direction * camera_speed;
    } else if (game->global_data->keys_state[KEYCODE_A]) {
        game->global_data->camera_position -= camera_right * camera_speed;
    } else if (game->global_data->keys_state[KEYCODE_D]) {
        game->global_data->camera_position += camera_right * camera_speed;
    }

    PERSIST f32 player_velocity = 5.0f;
    f32 player_speed = dt * player_velocity;

    if (game->global_data->keys_state[KEYCODE_Up]) {
        game->self->position.z -= player_speed;
        game->player_moved = true;
    } else if (game->global_data->keys_state[KEYCODE_Down]) {
        game->self->position.z += player_speed;
        game->player_moved = true;
    } else if (game->global_data->keys_state[KEYCODE_Left]) {
        game->self->position.x -= player_speed;
        game->player_moved = true;
    } else if (game->global_data->keys_state[KEYCODE_Right]) {
        game->self->position.x += player_speed;
        game->player_moved = true;
    }

    PERSIST f32 acc_updt = 0.0f;
    acc_updt += dt;
    if (acc_updt >= game->global_data->client_update_period && game->player_moved) {
        game->player_moved = false;
        acc_updt = 0.0f;

        Packet_Player_Move packet = {};
        packet.id = game->self->id;
        memcpy(packet.position, glm::value_ptr(game->self->position), 3 * sizeof(f32));

        if (!packet_send(game->global_data->client_socket, PACKET_TYPE_PLAYER_MOVE, &packet)) {
            LOG_ERROR("failed to send player move packet\n");
        }
    }
}

// This module is hot-reloadable using dlopen
// We need to prevent function name mangling by the C++ compiler,
// in order to retrieve functions by name using dlsym
extern "C" {

void game_post_reload(Game *game)
{
    net_init(game->global_data->ns);
    mem_init(game->global_data->ms);
    log_init(game->global_data->lr);
    event_system_init(game->global_data->re);
}

void game_init(Game *game)
{
    game_post_reload(game);

    bool shader_create_result;
    UNUSED(shader_create_result);

    Shader_Create_Info flat_color_shader_create_info = {
        .vertex_filepath = "assets/shaders/flat_color.vert",
        .fragment_filepath = "assets/shaders/flat_color.frag"
    };

    shader_create_result = shader_create(&flat_color_shader_create_info, &game->flat_color_shader);
    ASSERT(shader_create_result);

    Shader_Create_Info lighting_shader_create_info = {
        .vertex_filepath = "assets/shaders/lighting.vert",
        .fragment_filepath = "assets/shaders/lighting.frag"
    };

    shader_create_result = shader_create(&lighting_shader_create_info, &game->lighting_shader);
    ASSERT(shader_create_result);

    Shader_Create_Info voxel_shader_create_info = {
        .vertex_filepath = "assets/shaders/voxel.vert",
        .fragment_filepath = "assets/shaders/voxel.frag"
    };

    shader_create_result = shader_create(&voxel_shader_create_info, &game->voxel_shader);
    ASSERT(shader_create_result);

    bool skybox_create_result;
    UNUSED(skybox_create_result);

    game->skybox = (Skybox *) mem_alloc(sizeof(Skybox), MEMORY_TAG_GAME);

    Skybox_Create_Info skybox_create_info = {
        .debug = true,
        .face_filepaths = {
            "assets/textures/prototypes/texture_06.png",
        }
    };

    skybox_create_result = skybox_create(&skybox_create_info, game->skybox);
    ASSERT(skybox_create_result);

    f32 vertices[] = {
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
         0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,
        -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,

        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
        -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f
    };

    glGenVertexArrays(1, &game->vao);
    glBindVertexArray(game->vao);

    glGenBuffers(1, &game->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, game->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(f32), (const void *) 0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(f32), (const void *) (3 * sizeof(f32)));
    glEnableVertexAttribArray(1);

    game->voxel_data = (Voxel_Data *) mem_alloc(num_instances * sizeof(Voxel_Data), MEMORY_TAG_GAME);
    ASSERT_MSG(game->voxel_data != NULL, "failed to allocate memory for voxel models");

    u32 count = 0;
    for (i32 z = -50; z < 50; z++) {
        for (i32 x = -50; x < 50; x++) {
            Voxel_Data data = {
                .model = glm::translate(glm::mat4(1.0f), glm::vec3(1.2f * (f32) x, -1.0f, 1.2f * (f32) z)),
                .color = glm::vec3(0.6f, 0.6f, 0.2f)
            };

            game->voxel_data[count] = data;
            count++;
        }
    }

    glGenBuffers(1, &game->inst_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, game->inst_vbo);
    glBufferData(GL_ARRAY_BUFFER, num_instances * sizeof(Voxel_Data), game->voxel_data, GL_STATIC_DRAW);

    // model instance attributes
    i32 vec4_size = (i32) sizeof(glm::vec4);
    for (i32 i = 0; i < 4; i++) {
        glVertexAttribPointer(2 + i, 4, GL_FLOAT, GL_FALSE, sizeof(Voxel_Data), (const void *) (i * (u64)vec4_size));
        glEnableVertexAttribArray(2 + i);
        glVertexAttribDivisor(2 + i, 1);
    }

    // color instance attributes
    glVertexAttribPointer(6, 3, GL_FLOAT, GL_FALSE, sizeof(Voxel_Data), (const void *) (4 * (u64)vec4_size));
    glEnableVertexAttribArray(6);
    glVertexAttribDivisor(6, 1);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

void game_update(Game *game, f32 dt)
{
    process_input(game, dt);

    glm::mat4 projection = glm::perspective(glm::radians(game->global_data->camera_fov), (f32) game->global_data->current_window_width / (f32) game->global_data->current_window_height, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(game->global_data->camera_position, game->global_data->camera_position + game->global_data->camera_direction, game->global_data->camera_up);

    if (game->global_data->skybox_visible) {
        skybox_render(game->skybox, &projection, &view);
    }

    shader_bind(&game->lighting_shader);
    shader_set_uniform_mat4(&game->lighting_shader, "u_projection", &projection);
    shader_set_uniform_mat4(&game->lighting_shader, "u_view", &view);
    shader_set_uniform_vec3(&game->lighting_shader, "u_camera_pos", &game->global_data->camera_position);
    glm::vec3 mat_specular = glm::vec3(0.5f);
    shader_set_uniform_vec3(&game->lighting_shader, "u_material.specular", &mat_specular);
    shader_set_uniform_float(&game->lighting_shader, "u_material.shininess", 32.0f);

    glm::vec3 current_light_position = {};
    if (game->light.id != 0) {
        f32 t = clock_get_absolute_time_sec();
        current_light_position = glm::vec3(
            game->light.initial_position.x + glm::cos(game->light.angular_velocity * t) * game->light.radius,
            game->light.initial_position.y,
            game->light.initial_position.z + glm::sin(game->light.angular_velocity * t) * game->light.radius
        );

        shader_bind(&game->lighting_shader);
        shader_set_uniform_vec3(&game->lighting_shader, "u_light.position", &current_light_position);
        shader_set_uniform_vec3(&game->lighting_shader, "u_light.ambient", &game->light.ambient);
        shader_set_uniform_vec3(&game->lighting_shader, "u_light.diffuse", &game->light.diffuse);
        shader_set_uniform_vec3(&game->lighting_shader, "u_light.specular", &game->light.specular);

        shader_bind(&game->voxel_shader);
        shader_set_uniform_vec3(&game->voxel_shader, "u_light.position", &current_light_position);
        shader_set_uniform_vec3(&game->voxel_shader, "u_light.ambient", &game->light.ambient);
        shader_set_uniform_vec3(&game->voxel_shader, "u_light.diffuse", &game->light.diffuse);
        shader_set_uniform_vec3(&game->voxel_shader, "u_light.specular", &game->light.specular);
    }

    glBindVertexArray(game->vao);

    // Render voxels
    shader_bind(&game->voxel_shader);
    shader_set_uniform_mat4(&game->voxel_shader, "u_projection", &projection);
    shader_set_uniform_mat4(&game->voxel_shader, "u_view", &view);
    shader_set_uniform_vec3(&game->voxel_shader, "u_camera_pos", &game->global_data->camera_position);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 36, 100 * 100);

    shader_bind(&game->lighting_shader);

    // Render players
    for (Player *player = game->players; player != NULL; player = (Player *) player->hh.next) {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), player->position);
        shader_set_uniform_mat4(&game->lighting_shader, "u_model", &model);
        shader_set_uniform_vec3(&game->lighting_shader, "u_material.ambient", &player->color);
        shader_set_uniform_vec3(&game->lighting_shader, "u_material.diffuse", &player->color);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    {
        // Render ourselves
        glm::mat4 model = glm::translate(glm::mat4(1.0f), game->self->position);
        shader_set_uniform_mat4(&game->lighting_shader, "u_model", &model);
        shader_set_uniform_vec3(&game->lighting_shader, "u_material.ambient", &game->self->color);
        shader_set_uniform_vec3(&game->lighting_shader, "u_material.diffuse", &game->self->color);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    if (game->light.id != 0) {
        shader_bind(&game->flat_color_shader);

        glm::mat4 light_model = glm::translate(glm::mat4(1.0f), current_light_position) * glm::scale(glm::mat4(1.0f), glm::vec3(0.25f));
        shader_set_uniform_mat4(&game->flat_color_shader, "u_projection", &projection);
        shader_set_uniform_mat4(&game->flat_color_shader, "u_view", &view);
        shader_set_uniform_mat4(&game->flat_color_shader, "u_model", &light_model);
        glm::vec4 color = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f);
        shader_set_uniform_vec4(&game->flat_color_shader, "u_color", &color);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    renderer2d_begin_scene(game->global_data->renderer2d, &game->global_data->ui_projection);
    renderer2d_draw_line(game->global_data->renderer2d, glm::vec2(-10.0f, 0.0f), glm::vec2(10.0f, 0.0f), glm::vec3(1.0f));
    renderer2d_draw_line(game->global_data->renderer2d, glm::vec2(0.0f, -10.0f), glm::vec2(0.0f, 10.0f), glm::vec3(1.0f));
    renderer2d_end_scene(game->global_data->renderer2d);
}

void game_shutdown(Game *game)
{
    shader_destroy(&game->flat_color_shader);
    shader_destroy(&game->lighting_shader);
    shader_destroy(&game->voxel_shader);
    glDeleteVertexArrays(1, &game->vao);
    glDeleteBuffers(1, &game->vbo);
    glDeleteBuffers(1, &game->inst_vbo);
    mem_free(game->voxel_data, num_instances * sizeof(Voxel_Data), MEMORY_TAG_GAME);
    skybox_destroy(game->skybox);
    mem_free(game->skybox, sizeof(Skybox), MEMORY_TAG_GAME);
}

} // extern "C"
