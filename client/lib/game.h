#pragma once

#include <stdint.h>

#include <glm/glm.hpp>

#include "client/renderer2d.h"
#include "client/input_codes.h"
#include "common/net.h"
#include "common/defines.h"
#include "common/player_types.h"
#include "common/entity_types.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

struct GLFWwindow;

typedef struct {
    glm::mat4 model;
    glm::vec3 color;
} Voxel_Data;

typedef struct {
    Net_Stat *ns;
    GLFWwindow *window;
    u32 current_window_width;
    u32 current_window_height;
    bool is_polygon_mode;
    bool player_moved;
    glm::mat4 ui_projection;
    glm::vec3 camera_position;
    glm::vec3 camera_direction;
    glm::vec3 camera_up;
    f32 camera_fov;
    f32 camera_pitch;
    f32 camera_yaw;
    u32 vao, vbo, inst_vbo;
    Voxel_Data *voxel_data;
    Shader flat_color_shader;
    Shader lighting_shader;
    Shader voxel_shader;
    Light light;
    Player *players; // uthash
    Player *self;
    i32 client_socket;
    f32 client_update_freq;
    f32 client_update_period;
    Renderer2D *renderer2d;
    bool keys_state[KEYCODE_Last];
} Game;

typedef void (*pfn_game_init)(Game *game);
typedef void (*pfn_game_update)(Game *game, f32 dt);
typedef void (*pfn_game_shutdown)(Game *game);

#define LIST_OF_GAME_HRFN \
    X(game_init) \
    X(game_update) \
    X(game_shutdown)
