#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <dlfcn.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

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
            fprintf(stderr, "error closing shared object: %s\n", dlerror());
            return false;
        }
    }

    libgame = dlopen(libgame_filename, RTLD_NOW);
    if (libgame == NULL) {
        fprintf(stderr, "error: dlopen could not load %s\n", dlerror());
        return false;
    }

    #define X(name) \
        name = (pfn_##name) dlsym(libgame, #name); \
        if (name == NULL) { \
            fprintf(stderr, "error: dlsym: %s\n", dlerror()); \
            return false; \
        }
    LIST_OF_GAME_HRFN
    #undef X

    return true;
}

static void glfw_error_callback(int code, const char *description)
{
    fprintf(stderr, "error: glfw error (%d): %s\n", code, description);
}

static void glfw_framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    UNUSED(window);
    glViewport(0, 0, width, height);
    game.current_window_width = width;
    game.current_window_height = height;
}

static void glfw_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    UNUSED(window);
    UNUSED(scancode);
    UNUSED(mods);

    if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        game.is_polygon_mode = !game.is_polygon_mode;
        int32_t mode = game.is_polygon_mode ? GL_LINE : GL_FILL;
        glPolygonMode(GL_FRONT_AND_BACK, mode);
    } else if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
            if (!reload_libgame()) {
                exit(1);
            }
            printf("successfully reloaded libgame\n");
        }
    }
}

static void glfw_mouse_moved_callback(GLFWwindow *window, double xpos, double ypos)
{
    UNUSED(window);

    static bool first_mouse_move = true;
    static float last_x, last_y;

    if (first_mouse_move) {
        last_x = (float) xpos;
        last_y = (float) ypos;
        first_mouse_move = false;
    }

    float xoffset = (float) xpos - last_x;
    float yoffset = last_y - (float) ypos;
    last_x = (float) xpos;
    last_y = (float) ypos;

    static float sensitivity = 0.1f;
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
        return 1;
    }
    printf("successfully loaded libgame\n");

    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        fprintf(stderr, "error: failed to initialize glfw\n");
        return 1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    const char *glfw_version = glfwGetVersionString();
    printf("glfw version: %s\n", glfw_version);

    game.window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "voxland client", NULL, NULL);
    if (!game.window) {
        fprintf(stderr, "error: failed to create glfw window\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(game.window);
    glfwSetFramebufferSizeCallback(game.window, glfw_framebuffer_size_callback);
    glfwSetKeyCallback(game.window, glfw_key_callback);
    glfwSetCursorPosCallback(game.window, glfw_mouse_moved_callback);

    glfwSetInputMode(game.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    uint32_t err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "error: failed to initialize glew: %s\n", glewGetErrorString(err));
        glfwDestroyWindow(game.window);
        glfwTerminate();
        return 1;
    }

    {
        const unsigned char *gl_vendor = glGetString(GL_VENDOR);
        const unsigned char *gl_renderer = glGetString(GL_RENDERER);
        const unsigned char *gl_version = glGetString(GL_VERSION);
        const unsigned char *glsl_version = glGetString(GL_SHADING_LANGUAGE_VERSION);
        printf("graphics info:\n");
        printf("  vendor: %s\n", gl_vendor);
        printf("  renderer: %s\n", gl_renderer);
        printf("  opengl version: %s\n", gl_version);
        printf("  glsl version: %s\n", glsl_version);
    }

    game_init(&game);

    while (!glfwWindowShouldClose(game.window)) {
        float now = (float) glfwGetTime();
        game.delta_time = now - game.last_time;
        game.last_time = now;

        game_update(&game);
    }

    game_shutdown(&game);

    glfwDestroyWindow(game.window);
    glfwTerminate();

    return 0;
}
