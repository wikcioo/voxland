#pragma once

#include <stdint.h>

#include <glm/glm.hpp>

#include "defines.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

struct GLFWwindow;

typedef struct {
    GLFWwindow *window;
    u32 current_window_width;
    u32 current_window_height;
    bool is_polygon_mode;
    f32 delta_time;
    f32 last_time;
    glm::vec3 camera_position;
    glm::vec3 camera_direction;
    glm::vec3 camera_up;
    f32 camera_fov;
    f32 camera_pitch;
    f32 camera_yaw;
    u32 vao, vbo;
    u32 flat_color_shader;
    u32 lighting_shader;
} Game;

typedef void (*pfn_game_init)(Game *game);
typedef void (*pfn_game_update)(Game *game);
typedef void (*pfn_game_shutdown)(Game *game);

#define LIST_OF_GAME_HRFN \
    X(game_init) \
    X(game_update) \
    X(game_shutdown)
