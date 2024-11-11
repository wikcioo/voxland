#pragma once

#include <glm/glm.hpp>

#include "client/global.h"
#include "client/shader.h"
#include "client/skybox.h"
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
    Global_Data *global_data;
    bool player_moved;
    u32 vao, vbo, inst_vbo;
    Voxel_Data *voxel_data;
    Shader flat_color_shader;
    Shader lighting_shader;
    Shader voxel_shader;
    Light light;
    Player *players; // uthash
    Player *self;
    Skybox *skybox;
} Game;

typedef void (*pfn_game_post_reload)(Game *game);
typedef void (*pfn_game_init)(Game *game);
typedef void (*pfn_game_update)(Game *game, f32 dt);
typedef void (*pfn_game_shutdown)(Game *game);

#define LIST_OF_GAME_HRFN \
    X(game_post_reload) \
    X(game_init) \
    X(game_update) \
    X(game_shutdown)
