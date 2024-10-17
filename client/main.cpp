#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <poll.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "log.h"
#include "asserts.h"
#include "defines.h"
#include "game.h"
#include "hexdump.h"

#define POLLFD_COUNT 1
#define INPUT_BUFFER_SIZE 1024
#define POLL_INFINITE_TIMEOUT -1

LOCAL Game game = {};
LOCAL i32 client_socket;
LOCAL struct pollfd pfds[POLLFD_COUNT];
LOCAL bool running = false;
LOCAL pthread_t network_thread;

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

LOCAL void signal_handler(i32 sig)
{
    UNUSED(sig);
    running = false;
}

LOCAL void handle_incoming_server_data(void)
{
    u8 buffer[INPUT_BUFFER_SIZE] = {0};
    i64 bytes_read = recv(client_socket, buffer, INPUT_BUFFER_SIZE, 0);
    if (bytes_read <= 0) {
        if (bytes_read == 0) {
            LOG_INFO("orderly shutdown: disconnected from server\n");
            close(client_socket);
            exit(EXIT_FAILURE);
        } else if (bytes_read == -1) {
            LOG_ERROR("recv error: %s\n", strerror(errno));
        }
        return;
    }

#if 1
    LOG_DEBUG("read %lld bytes of data:\n", bytes_read);
    hexdump(stdout, buffer, bytes_read, HEXDUMP_FLAG_CANONICAL);
#endif
}

LOCAL void *handle_networking(void *args)
{
    UNUSED(args);

    while (running) {
        i32 num_events = poll(pfds, POLLFD_COUNT, POLL_INFINITE_TIMEOUT);

        if (num_events == -1) {
            if (errno == EINTR) {
                LOG_TRACE("interrupted 'poll' system call\n");
                break;
            }
            LOG_FATAL("poll error: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        for (i32 i = 0; i < POLLFD_COUNT; i++) {
            if (pfds[i].revents & POLLIN) {
                if (pfds[i].fd == client_socket) {
                    handle_incoming_server_data();
                }
            }
        }
    }

    if (close(client_socket) == -1) {
        LOG_ERROR("error while closing the socket: %s\n", strerror(errno));
    } else {
        LOG_INFO("closed client socket\n");
    }

    return NULL;
}

LOCAL bool connect_to_server(const char *ip_addr, const char *port)
{
    struct addrinfo hints = {}, *result = NULL, *rp = NULL;

    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    i32 rc = getaddrinfo(ip_addr, port, &hints, &result);
    if (rc != 0) {
        LOG_ERROR("getaddrinfo error: %s\n", gai_strerror(rc));
        return false;
    }

    for (rp = result; rp != NULL; rp = rp->ai_next) {
        client_socket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (client_socket == -1) {
            continue;
        }

        if (connect(client_socket, rp->ai_addr, rp->ai_addrlen) == -1) {
            LOG_ERROR("connect error: %s\n", strerror(errno));
            continue;
        }

        break;
    }

    freeaddrinfo(result);

    if (rp == NULL) {
        return false;
    }

    pfds[0].fd = client_socket;
    pfds[0].events = POLLIN;

    struct sigaction sa;
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = &signal_handler;
    sigaction(SIGINT, &sa, NULL);

    pthread_create(&network_thread, NULL, handle_networking, NULL);

    return true;
}

LOCAL char *shift(int *argc, char ***argv)
{
    ASSERT(*argc > 0);
    char *result = **argv;
    *argc -= 1;
    *argv += 1;
    return result;
}

LOCAL void usage(FILE *stream, const char *const program)
{
    fprintf(stream, "usage: %s -ip <server ip address> -p <port> [-h]\n", program);
}

int main(int argc, char **argv)
{
    const char *const program = shift(&argc, &argv);
    const char *server_ip_address = NULL;
    const char *server_port = NULL;

    while (argc > 0) {
        const char *flag = shift(&argc, &argv);

        if (strcmp(flag, "-ip") == 0) {
            if (argc == 0) {
                LOG_FATAL("missing argument for flag `%s`\n", flag);
                usage(stderr, program);
                exit(EXIT_FAILURE);
            }

            server_ip_address = shift(&argc, &argv);
        } else if (strcmp(flag, "-p") == 0) {
            if (argc == 0) {
                LOG_FATAL("missing argument for flag `%s`\n", flag);
                usage(stderr, program);
                exit(EXIT_FAILURE);
            }

            server_port = shift(&argc, &argv);
        } else if (strcmp(flag, "-h") == 0) {
            usage(stdout, program);
            exit(EXIT_SUCCESS);
        } else {
            LOG_FATAL("unknown flag `%s`\n", flag);
            usage(stderr, program);
            exit(EXIT_FAILURE);
        }
    }

    if (server_ip_address == NULL) {
        LOG_FATAL("server ip address argument missing\n");
        usage(stderr, program);
        exit(EXIT_FAILURE);
    }

    if (server_port == NULL) {
        LOG_FATAL("server port argument missing\n");
        usage(stderr, program);
        exit(EXIT_FAILURE);
    }

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

    running = true;

    if (connect_to_server(server_ip_address, server_port)) {
        LOG_INFO("successfully connected to server at %s:%s\n", server_ip_address, server_port);
    } else {
        LOG_FATAL("failed to connect to server at %s:%s\n", server_ip_address, server_port);
        exit(EXIT_FAILURE);
    }

    game_init(&game);

    while (!glfwWindowShouldClose(game.window) && running) {
        f32 now = (f32) glfwGetTime();
        game.delta_time = now - game.last_time;
        game.last_time = now;

        game_update(&game);
    }

    game_shutdown(&game);

    glfwDestroyWindow(game.window);
    glfwTerminate();

    pthread_kill(network_thread, SIGINT);
    pthread_join(network_thread, NULL);

    return EXIT_SUCCESS;
}
