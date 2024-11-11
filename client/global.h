#pragma once

#include <glm/glm.hpp>

#include "common/log.h"
#include "common/net.h"
#include "common/event.h"
#include "common/defines.h"
#include "client/renderer2d.h"
#include "client/input_codes.h"
#include "common/memory/memutils.h"

#define WINDOW_WIDTH 1280
#define WINDOW_HEIGHT 720

struct GLFWwindow;

// Data available across the entire client application.
typedef struct {
    Net_Stat *ns;
    Memory_Stats *ms;
    Log_Registry *lr;
    Registered_Event (*re)[NUM_OF_EVENT_CODES];
    GLFWwindow *window;
    u32 current_window_width;
    u32 current_window_height;
    bool is_polygon_mode;
    glm::mat4 ui_projection;
    glm::vec3 camera_position;
    glm::vec3 camera_direction;
    glm::vec3 camera_up;
    f32 camera_fov;
    f32 camera_pitch;
    f32 camera_yaw;
    f32 camera_speed_factor;
    i32 client_socket;
    f32 client_update_freq;
    f32 client_update_period;
    Renderer2D *renderer2d;
    bool keys_state[KEYCODE_Last];
    bool skybox_visible;
} Global_Data;

extern Global_Data global_data;
