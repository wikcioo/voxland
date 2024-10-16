#pragma once

#include <stdint.h>

#include <glm/glm.hpp>

#define UNUSED(x) ((void) x)

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

struct GLFWwindow;

typedef struct {
    GLFWwindow *window;
    uint32_t current_window_width;
    uint32_t current_window_height;
    bool is_polygon_mode;
    float delta_time;
    float last_time;
    glm::vec3 camera_position;
    glm::vec3 camera_direction;
    glm::vec3 camera_up;
    float camera_fov;
    float camera_pitch;
    float camera_yaw;
    uint32_t vao, vbo;
    uint32_t flat_color_shader;
    uint32_t lighting_shader;
} Game;

typedef void (*pfn_game_init)(Game *game);
typedef void (*pfn_game_update)(Game *game);
typedef void (*pfn_game_shutdown)(Game *game);

#define LIST_OF_GAME_HRFN \
    X(game_init) \
    X(game_update) \
    X(game_shutdown)
