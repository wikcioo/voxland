#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <dlfcn.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "log.h"
#include "defines.h"
#include "game.h"

Game game = {};

const char *libgame_filename = "libgame.so";
void *libgame = NULL;

#define X(name) pfn_##name name = NULL;
LIST_OF_GAME_HRFN
#undef X

bool reload_libgame(void)
{
    if (libgame != NULL) {
        if (dlclose(libgame) != 0) {
            LOG_ERROR("error closing shared object: %s\n", dlerror());
            return false;
        }
    }

    libgame = dlopen(libgame_filename, RTLD_NOW);
    if (libgame == NULL) {
        LOG_ERROR("dlopen could not load %s\n", dlerror());
        return false;
    }

    #define X(name) \
        name = (pfn_##name) dlsym(libgame, #name); \
        if (name == NULL) { \
            LOG_ERROR("dlsym: %s\n", dlerror()); \
            return false; \
        }
    LIST_OF_GAME_HRFN
    #undef X

    return true;
}

LOCAL void glfw_error_callback(i32 code, const char *description)
{
    LOG_ERROR("glfw error (%d): %s\n", code, description);
}

LOCAL void glfw_framebuffer_size_callback(GLFWwindow *window, i32 width, i32 height)
{
    UNUSED(window);
    glViewport(0, 0, width, height);
    game.current_window_width = width;
    game.current_window_height = height;
}

LOCAL void glfw_key_callback(GLFWwindow *window, i32 key, i32 scancode, i32 action, i32 mods)
{
    UNUSED(window);
    UNUSED(scancode);
    UNUSED(mods);

    if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        game.is_polygon_mode = !game.is_polygon_mode;
        i32 mode = game.is_polygon_mode ? GL_LINE : GL_FILL;
        glPolygonMode(GL_FRONT_AND_BACK, mode);
    } else if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
            if (!reload_libgame()) {
                LOG_FATAL("failed to reload libgame\n");
                exit(EXIT_FAILURE);
            }
            LOG_INFO("successfully reloaded libgame\n");
        }
    }
}

LOCAL void glfw_mouse_moved_callback(GLFWwindow *window, f64 xpos, f64 ypos)
{
    UNUSED(window);

    PERSIST bool first_mouse_move = true;
    PERSIST f32 last_x, last_y;

    if (first_mouse_move) {
        last_x = (f32) xpos;
        last_y = (f32) ypos;
        first_mouse_move = false;
    }

    f32 xoffset = (f32) xpos - last_x;
    f32 yoffset = last_y - (f32) ypos;
    last_x = (f32) xpos;
    last_y = (f32) ypos;

    PERSIST f32 sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    game.camera_yaw += xoffset;
    game.camera_pitch += yoffset;

    if (game.camera_pitch > 89.0f) {
        game.camera_pitch = 89.0f;
    } else if (game.camera_pitch < -89.0f) {
        game.camera_pitch = -89.0f;
    }

    glm::vec3 direction;
    direction.x = glm::cos(glm::radians(game.camera_yaw)) * glm::cos(glm::radians(game.camera_pitch));
    direction.y = glm::sin(glm::radians(game.camera_pitch));
    direction.z = glm::sin(glm::radians(game.camera_yaw)) * glm::cos(glm::radians(game.camera_pitch));
    game.camera_direction = glm::normalize(direction);
}

int main(void)
{
    if (!reload_libgame()) {
        LOG_FATAL("failed to reload libgame\n");
        exit(EXIT_FAILURE);
    }
    LOG_INFO("successfully loaded libgame\n");

    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        LOG_FATAL("failed to initialize glfw\n");
        exit(EXIT_FAILURE);
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    const char *glfw_version = glfwGetVersionString();
    LOG_INFO("glfw version: %s\n", glfw_version);

    game.window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "voxland client", NULL, NULL);
    if (!game.window) {
        LOG_FATAL("failed to create glfw window\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(game.window);
    glfwSetFramebufferSizeCallback(game.window, glfw_framebuffer_size_callback);
    glfwSetKeyCallback(game.window, glfw_key_callback);
    glfwSetCursorPosCallback(game.window, glfw_mouse_moved_callback);

    glfwSetInputMode(game.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    u32 err = glewInit();
    if (err != GLEW_OK) {
        LOG_FATAL("failed to initialize glew: %s\n", glewGetErrorString(err));
        glfwDestroyWindow(game.window);
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    {
        const unsigned char *gl_vendor = glGetString(GL_VENDOR);
        const unsigned char *gl_renderer = glGetString(GL_RENDERER);
        const unsigned char *gl_version = glGetString(GL_VERSION);
        const unsigned char *glsl_version = glGetString(GL_SHADING_LANGUAGE_VERSION);
        LOG_INFO("graphics info:\n");
        LOG_INFO("  - vendor: %s\n", gl_vendor);
        LOG_INFO("  - renderer: %s\n", gl_renderer);
        LOG_INFO("  - opengl version: %s\n", gl_version);
        LOG_INFO("  - glsl version: %s\n", glsl_version);
    }

    game_init(&game);

    while (!glfwWindowShouldClose(game.window)) {
        f32 now = (f32) glfwGetTime();
        game.delta_time = now - game.last_time;
        game.last_time = now;

        game_update(&game);
    }

    game_shutdown(&game);

    glfwDestroyWindow(game.window);
    glfwTerminate();

    return 0;
}
