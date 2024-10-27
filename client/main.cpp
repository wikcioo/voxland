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

#include <glm/gtc/type_ptr.hpp>

#include "game.h"
#include "common/log.h"
#include "common/asserts.h"
#include "common/defines.h"
#include "common/hexdump.h"
#include "common/packet.h"
#include "common/clock.h"

#define POLLFD_COUNT 1
#define INPUT_BUFFER_SIZE 1024
#define INPUT_OVERFLOW_BUFFER_SIZE 1024
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

LOCAL void ping_server(void)
{
    ASSERT(client_socket > 2);

    u64 time_now = clock_get_absolute_time_ns();
    Packet_Ping ping_packet = {
        .time = time_now
    };

    if (!packet_send(client_socket, PACKET_TYPE_PING, &ping_packet)) {
        LOG_ERROR("failed to send ping packet\n");
    }
}

LOCAL void send_text_message_to_server(const char *message)
{
    ASSERT(client_socket > 2);

    Packet_Text_Message packet = {
        .length = (u32) strlen(message),
        .message = (char *) message
    };

    if (!packet_send(client_socket, PACKET_TYPE_TXT_MSG, &packet)) {
        LOG_ERROR("failed to send text message packet\n");
    }
}

LOCAL void glfw_error_callback(i32 code, const char *description)
{
    UNUSED(code); UNUSED(description);
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
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
            ping_server();
        } else {
            game.is_polygon_mode = !game.is_polygon_mode;
            i32 mode = game.is_polygon_mode ? GL_LINE : GL_FILL;
            glPolygonMode(GL_FRONT_AND_BACK, mode);
        }
    } else if (key == GLFW_KEY_R && action == GLFW_PRESS) {
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
            if (!reload_libgame()) {
                LOG_FATAL("failed to reload libgame\n");
                exit(EXIT_FAILURE);
            }
            LOG_INFO("successfully reloaded libgame\n");
        }
    } else if (key == GLFW_KEY_M && action == GLFW_PRESS) {
        send_text_message_to_server("hello server!");
    } else if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {
        // Intentionally crash the client.
        i32 a = 1, b = 0;
        i32 c = a / b;
        UNUSED(c);
    }
}

LOCAL void glfw_mouse_moved_callback(GLFWwindow *window, f64 xpos, f64 ypos)
{
    PERSIST bool first_mouse_move = true;

    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) != GLFW_PRESS) {
        first_mouse_move = true;
        return;
    }

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

LOCAL void process_network_packet(u32 type, void *data)
{
    switch (type) {
        case PACKET_TYPE_NONE: {
            LOG_WARN("ignoring received packet of type PACKET_TYPE_NONE\n");
        } break;
        case PACKET_TYPE_PING: {
            Packet_Ping *packet = (Packet_Ping *) data;
            u64 time_now = clock_get_absolute_time_ns();
            f64 ping_ms = (f64) (time_now - packet->time) / 1000000.0;
            LOG_TRACE("ping = %fms\n", ping_ms);
        } break;
        case PACKET_TYPE_PLAYER_JOIN_RES: {
            Packet_Player_Join_Res *packet = (Packet_Player_Join_Res *) data;
            if (!packet->approved) {
                LOG_ERROR("client join request has been rejected by the server\n");
                // For now just terminate the client.
                close(client_socket);
                exit(EXIT_FAILURE);
                return;
            }

            game.self->id = packet->id;
            memcpy(glm::value_ptr(game.self->color), packet->color, 3 * sizeof(f32));
            memcpy(glm::value_ptr(game.self->position), packet->position, 3 * sizeof(f32));
            LOG_DEBUG("approved by the server with id=%u color=(%f,%f,%f)\n", packet->id, packet->color[0], packet->color[1], packet->color[2]);
        } break;
        case PACKET_TYPE_PLAYER_ADD: {
            Packet_Player_Add *packet = (Packet_Player_Add *) data;

            Player *player;
            HASH_FIND_INT(game.players, &packet->id, player);
            if (player != NULL) {
                LOG_ERROR("player with id=%u already exists\n", packet->id);
                break;
            }

            player = (Player *) malloc(sizeof(*player));
            memset(player, 0, sizeof(*player));
            // TODO: move player init to a function
            player->id = packet->id;
            memcpy(player->username, packet->username, packet->username_length);
            player->color = glm::vec3(packet->color[0], packet->color[1], packet->color[2]);
            player->position = glm::vec3(packet->position[0], packet->position[1], packet->position[2]);
            HASH_ADD_INT(game.players, id, player);
            LOG_DEBUG("added player: username='%s' id=%u position=(%f,%f,%f)\n", player->username, player->id, player->position.x, player->position.y, player->position.z);
        } break;
        case PACKET_TYPE_PLAYER_REMOVE: {
            Packet_Player_Remove *packet = (Packet_Player_Remove *) data;
            Player *player;
            HASH_FIND_INT(game.players, &packet->id, player);
            if (player == NULL) {
                LOG_ERROR("failed to find player with id=%u to remove\n", packet->id);
                break;
            }
            LOG_DEBUG("removed player: username='%s' id=%u\n", player->username, player->id);
            HASH_DEL(game.players, player);
            free(player); // NOTE: maybe we want to keep the data for further use
        } break;
        case PACKET_TYPE_PLAYER_MOVE: {
            Packet_Player_Move *packet = (Packet_Player_Move *) data;
            Player *player = NULL;
            HASH_FIND_INT(game.players, &packet->id, player);
            if (player == NULL) {
                LOG_ERROR("failed to find player with id=%u to move\n", packet->id);
                break;
            }

            memcpy(glm::value_ptr(player->position), packet->position, 3 * sizeof(f32));
        } break;
        case PACKET_TYPE_PLAYER_BATCH_MOVE: {
            Packet_Player_Batch_Move packet;
            deserialize_packet_player_batch_move(data, &packet);

            for (u32 i = 0; i < packet.count; i++) {
                Player *player = NULL;
                HASH_FIND_INT(game.players, &packet.ids[i], player);
                if (player == NULL) {
                    if (packet.ids[i] != game.self->id) {
                        LOG_ERROR("failed to find player with id=%u to move\n", packet.ids[i]);
                    }
                    break;
                }

                memcpy(glm::value_ptr(player->position), &packet.positions[i * 3], 3 * sizeof(f32));
            }
        } break;
        case PACKET_TYPE_LIGHT_UPDATE: {
            Packet_Light_Update *packet = (Packet_Light_Update *) data;
            game.light.id = packet->id;
            game.light.radius = packet->radius;
            game.light.angular_velocity = packet->angular_velocity;
            game.light.initial_position = glm::vec3(packet->initial_position[0], packet->initial_position[1], packet->initial_position[2]);
            game.light.ambient = glm::vec3(packet->ambient[0], packet->ambient[1], packet->ambient[2]);
            game.light.diffuse = glm::vec3(packet->diffuse[0], packet->diffuse[1], packet->diffuse[2]);
            game.light.specular = glm::vec3(packet->specular[0], packet->specular[1], packet->specular[2]);
            LOG_DEBUG("light updated:\n");
            LOG_DEBUG("  - id: %u\n", game.light.id);
            LOG_DEBUG("  - radius: %f\n", game.light.radius);
            LOG_DEBUG("  - angular velocity: %f\n", game.light.angular_velocity);
            LOG_DEBUG("  - initial position: (%f,%f,%f)\n", game.light.initial_position.x, game.light.initial_position.y, game.light.initial_position.z);
            LOG_DEBUG("  - ambient color: (%f,%f,%f)\n", game.light.ambient.x, game.light.ambient.y, game.light.ambient.z);
            LOG_DEBUG("  - diffuse color: (%f,%f,%f)\n", game.light.diffuse.x, game.light.diffuse.y, game.light.diffuse.z);
            LOG_DEBUG("  - specular color: (%f,%f,%f)\n", game.light.specular.x, game.light.specular.y, game.light.specular.z);
        } break;
        default: {
            LOG_ERROR("unknown packet type value `%u`\n", type);
        }
    }
}

LOCAL void handle_incoming_server_data(void)
{
    u8 recv_buffer[INPUT_BUFFER_SIZE + INPUT_OVERFLOW_BUFFER_SIZE] = {0};
    i64 bytes_read = recv(client_socket, recv_buffer, INPUT_BUFFER_SIZE, 0);
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

#if 0
    LOG_DEBUG("read %lld bytes of data:\n", bytes_read);
    hexdump(stdout, recv_buffer, bytes_read, HEXDUMP_FLAG_CANONICAL);
#endif

    PERSIST const u32 header_size = PACKET_TYPE_SIZE[PACKET_TYPE_HEADER];

    ASSERT_MSG(bytes_read >= header_size, "unhandled: read fewer bytes than header size");

    i64 remaining_bytes_to_parse = bytes_read;
    u8 *bufptr = recv_buffer;

    // Iterate over all the packets included in single TCP data reception.
    // Optionally, read more data to complete the payload.
    for (;;) {
        Packet_Header *header = (Packet_Header *) bufptr;

        if (header->type >= NUM_OF_PACKET_TYPES) {
            LOG_ERROR("received unknown header type `%u`\n", header->type);
            break;
        }

        // Verify whether all the payload data is present in the receive buffer.
        // If it’s not, read the remaining bytes from the socket that are needed to complete the payload.
        if (remaining_bytes_to_parse - header_size < header->payload_size) {
            i64 missing_bytes = header->payload_size - (remaining_bytes_to_parse - header_size);

            LOG_DEBUG("reading %lu more bytes to complete payload\n", missing_bytes);

            ASSERT_MSG(missing_bytes <= INPUT_OVERFLOW_BUFFER_SIZE, "not enough space in overflow buffer");

            // Read into INPUT_OVERFLOW_BUFFER of the recv_buffer and proceed to packet interpretation.
            i64 new_bytes_read = recv(client_socket, &recv_buffer[INPUT_BUFFER_SIZE], missing_bytes, 0);
            UNUSED(new_bytes_read);
            ASSERT(new_bytes_read == missing_bytes);
        }

        process_network_packet(header->type, bufptr + header_size);

        // Check if there are more bytes to parse.
        u64 parsed_packet_size = header_size + header->payload_size;
        bufptr = (bufptr + parsed_packet_size);
        remaining_bytes_to_parse -= parsed_packet_size;
        if (remaining_bytes_to_parse > 0) {
            LOG_DEBUG("remaining_bytes_to_parse = %lu\n", remaining_bytes_to_parse);
            if (remaining_bytes_to_parse >= header_size) {
                // Enough unparsed bytes to read the header.
                Packet_Header *next_header = (Packet_Header *) bufptr;
                if (next_header->type <= PACKET_TYPE_NONE || next_header->type >= NUM_OF_PACKET_TYPES) {
                    break;
                }
                LOG_DEBUG("next header is valid\n");
            } else {
                // Not enough remaining unparsed bytes to read a complete header.
                ASSERT_MSG(0, "unhandled: fewer bytes than header size");
            }
        } else {
            // No more bytes to parse.
            break;
        }
    }
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

LOCAL bool handle_client_validation(i32 client)
{
    u64 puzzle_buffer;
    i64 bytes_read, bytes_sent;

    bytes_read = recv(client, (void *) &puzzle_buffer, sizeof(puzzle_buffer), 0); // TODO: Handle unresponsive server
    if (bytes_read <= 0) {
        if (bytes_read == -1) {
            LOG_ERROR("validation: recv error: %s\n", strerror(errno));
        } else if (bytes_read == 0) {
            LOG_ERROR("validation: orderly shutdown\n");
        }
        return false;
    }

    if (bytes_read == sizeof(puzzle_buffer)) {
        u64 answer = puzzle_buffer ^ 0xDEADBEEFCAFEBABE; // TODO: Come up with a better validation function
        bytes_sent = send(client, (void *) &answer, sizeof(answer), 0);
        if (bytes_sent == -1) {
            LOG_ERROR("validation: send error: %s\n", strerror(errno));
            return false;
        } else if (bytes_sent != sizeof(answer)) {
            LOG_ERROR("validation: failed to send %lu bytes of validation data\n", sizeof(answer));
            return false;
        }

        bool status_buffer;
        bytes_read = recv(client, (void *) &status_buffer, sizeof(status_buffer), 0);
        if (bytes_read <= 0) {
            if (bytes_read == -1) {
                LOG_ERROR("validation status: recv error: %s\n", strerror(errno));
            } else if (bytes_read == 0) {
                LOG_ERROR("validation status: orderly shutdown\n");
            }
            return false;
        }

        return status_buffer;
    }

    LOG_ERROR("validation: received incorrect number of bytes\n");
    return false;
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

    if (!handle_client_validation(client_socket)) {
        LOG_FATAL("failed client validation\n");
        close(client_socket);
        return false;
    }

    LOG_INFO("client successfully validated\n");

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
    fprintf(stream, "usage: %s -ip <server ip address> -p <port> --username <username> --password <password> [-h]\n", program);
}

int main(int argc, char **argv)
{
    const char *const program = shift(&argc, &argv);
    const char *server_ip_address = NULL;
    const char *server_port = NULL;
    const char *username = NULL;
    const char *password = NULL;

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
        } else if (strcmp(flag, "--username") == 0) {
            if (argc == 0) {
                LOG_FATAL("missing argument for flag `%s`\n", flag);
                usage(stderr, program);
                exit(EXIT_FAILURE);
            }

            username = shift(&argc, &argv);
        } else if (strcmp(flag, "--password") == 0) {
            if (argc == 0) {
                LOG_FATAL("missing argument for flag `%s`\n", flag);
                usage(stderr, program);
                exit(EXIT_FAILURE);
            }

            password = shift(&argc, &argv);
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

    if (username == NULL) {
        LOG_FATAL("username argument missing\n");
        usage(stderr, program);
        exit(EXIT_FAILURE);
    } else if (strlen(username) > PLAYER_USERNAME_MAX_LEN) {
        LOG_FATAL("username too long\n");
        exit(EXIT_FAILURE);
    }

    if (password == NULL) {
        LOG_FATAL("password argument missing\n");
        usage(stderr, program);
        exit(EXIT_FAILURE);
    } else if (strlen(password) > PLAYER_PASSWORD_MAX_LEN) {
        LOG_FATAL("password too long\n");
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

    const char *glfw_version = glfwGetVersionString(); UNUSED(glfw_version);
    LOG_INFO("glfw version: %s\n", glfw_version);

    char window_title[256] = {0};
    snprintf(window_title, 256, "voxland client - username: %s", username);
    game.window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, window_title, NULL, NULL);
    if (!game.window) {
        LOG_FATAL("failed to create glfw window\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    glfwMakeContextCurrent(game.window);
    glfwSetFramebufferSizeCallback(game.window, glfw_framebuffer_size_callback);
    glfwSetKeyCallback(game.window, glfw_key_callback);
    glfwSetCursorPosCallback(game.window, glfw_mouse_moved_callback);

    glfwSwapInterval(0);

    u32 err = glewInit();
    if (err != GLEW_OK) {
        LOG_FATAL("failed to initialize glew: %s\n", glewGetErrorString(err));
        glfwDestroyWindow(game.window);
        glfwTerminate();
        exit(EXIT_FAILURE);
    }

    {
        const unsigned char *gl_vendor    = glGetString(GL_VENDOR); UNUSED(gl_vendor);
        const unsigned char *gl_renderer  = glGetString(GL_RENDERER); UNUSED(gl_renderer);
        const unsigned char *gl_version   = glGetString(GL_VERSION); UNUSED(gl_version);
        const unsigned char *glsl_version = glGetString(GL_SHADING_LANGUAGE_VERSION); UNUSED(glsl_version);
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

    game.self = (Player *) malloc(sizeof(Player));
    memset(game.self, 0, sizeof(Player));
    game.client_socket = client_socket;
    game.client_update_freq = 60.0f;
    game.client_update_period = 1.0f / game.client_update_freq;
    game_init(&game);

    // Request to join
    Packet_Player_Join_Req join_req = {};
    join_req.username_length = (u8) strlen(username);
    memcpy(join_req.username, username, join_req.username_length);
    join_req.password_length = (u8) strlen(password);
    memcpy(join_req.password, password, join_req.password_length);

    if (!packet_send(client_socket, PACKET_TYPE_PLAYER_JOIN_REQ, &join_req)) {
        LOG_FATAL("failed to send player join request packet\n");
        exit(EXIT_FAILURE);
    }

    f32 delta_time = 0.0f;
    f32 last_time = 0.0f;
    while (!glfwWindowShouldClose(game.window) && running) {
        f32 now = (f32) glfwGetTime();
        delta_time = now - last_time;
        last_time = now;

        game_update(&game, delta_time);
    }

    game_shutdown(&game);

    Packet_Player_Remove player_remove_packet = { .id = game.self->id };
    if (!packet_send(client_socket, PACKET_TYPE_PLAYER_REMOVE, &player_remove_packet)) {
        LOG_ERROR("failed to send player remove packet\n");
    }

    free(game.self);

    {
        // uthash cleanup
        Player *p, *tmp1;
        HASH_ITER(hh, game.players, p, tmp1) {
            HASH_DEL(game.players, p);
            free(p);
        }
    }

    glfwDestroyWindow(game.window);
    glfwTerminate();

    pthread_kill(network_thread, SIGINT);
    pthread_join(network_thread, NULL);

    if (dlclose(libgame) != 0) {
        LOG_ERROR("error closing shared object: %s\n", dlerror());
    }

    return EXIT_SUCCESS;
}
