#include "game.h"

#include <stdio.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "common/log.h"
#include "common/packet.h"

const char *vertex_shader_source =
    "#version 330 core\n"
    "layout(location = 0) in vec3 a_position;\n"
    "layout(location = 1) in vec3 a_normal;\n"
    "out vec3 vs_position;\n"
    "out vec3 vs_normal;\n"
    "uniform mat4 projection;\n"
    "uniform mat4 view;\n"
    "uniform mat4 model;\n"
    "void main() {\n"
    "    gl_Position = projection * view * model * vec4(a_position, 1.0);\n"
    "    vs_position = vec3(model * vec4(a_position, 1.0));\n"
    "    vs_normal = mat3(transpose(inverse(model))) * a_normal;\n"
    "}\n";

const char *fragment_shader_source =
    "#version 330 core\n"
    "out vec4 out_color;\n"
    "uniform vec4 color;\n"
    "void main() {\n"
    "    out_color = color;\n"
    "}\n";

const char *fragment_shader_lighting_source =
    "#version 330 core\n"
    "out vec4 out_color;\n"
    "in vec3 vs_position;\n"
    "in vec3 vs_normal;\n"
    "struct material {\n"
    "    vec3 ambient;\n"
    "    vec3 diffuse;\n"
    "    vec3 specular;\n"
    "    float shininess;\n"
    "};\n"
    "struct point_light {\n"
    "    vec3 position;\n"
    "    vec3 ambient;\n"
    "    vec3 diffuse;\n"
    "    vec3 specular;\n"
    "};\n"
    "uniform material mat;\n"
    "uniform point_light light;\n"
    "uniform vec3 camera_pos;\n"
    "void main() {\n"
    "    vec3 ambient = light.ambient * mat.ambient;\n"
    "    vec3 normal = normalize(vs_normal);\n"
    "    vec3 light_dir = normalize(light.position - vs_position);\n"
    "    float diff = max(dot(normal, light_dir), 0.0);\n"
    "    vec3 diffuse = light.diffuse * (diff * mat.diffuse);\n"
    "    vec3 view_dir = normalize(camera_pos - vs_position);\n"
    "    vec3 reflect_dir = reflect(-light_dir, normal);\n"
    "    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), mat.shininess);\n"
    "    vec3 specular = light.specular * (spec * mat.specular);\n"
    "    vec3 result = ambient + diffuse + specular;\n"
    "    out_color = vec4(result, 1.0);\n"
    "}\n";

u32 create_shader(u32 type, const char *source)
{
    u32 shader = glCreateShader(type);
    glShaderSource(shader, 1, (const GLchar **)&source, 0);
    glCompileShader(shader);

    i32 is_compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &is_compiled);
    if (is_compiled == false) {
        char info_log[1024] = {0};
        glGetShaderInfoLog(shader, 1024, 0, info_log);
        const char *shader_type_as_cstr = type == GL_VERTEX_SHADER ? "vertex" : (type == GL_FRAGMENT_SHADER ? "fragment" : "unknown");
        LOG_FATAL("failed to compile %s shader: %s\n", shader_type_as_cstr, info_log);
        exit(EXIT_FAILURE);
    }

    return shader;
}

u32 create_program(u32 vertex_shader, u32 fragment_shader)
{
    u32 program = glCreateProgram();

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    i32 is_linked;
    glGetProgramiv(program, GL_LINK_STATUS, &is_linked);
    if (is_linked == false) {
        char info_log[1024];
        glGetProgramInfoLog(program, 1024, 0, info_log);
        LOG_FATAL("failed to link program: %s\n", info_log);
        exit(EXIT_FAILURE);
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return program;
}

void process_input(Game *game, f32 dt)
{
    PERSIST f32 velocity = 5.0f;
    f32 speed = dt * velocity;

    glm::vec3 camera_right = glm::normalize(glm::cross(game->camera_direction, game->camera_up));

    if (glfwGetKey(game->window, GLFW_KEY_W) == GLFW_PRESS) {
        game->camera_position += game->camera_direction * speed;
    } else if (glfwGetKey(game->window, GLFW_KEY_S) == GLFW_PRESS) {
        game->camera_position -= game->camera_direction * speed;
    } else if (glfwGetKey(game->window, GLFW_KEY_A) == GLFW_PRESS) {
        game->camera_position -= camera_right * speed;
    } else if (glfwGetKey(game->window, GLFW_KEY_D) == GLFW_PRESS) {
        game->camera_position += camera_right * speed;
    }

    if (glfwGetKey(game->window, GLFW_KEY_UP) == GLFW_PRESS) {
        game->self->position.z -= speed;
        game->player_moved = true;
    } else if (glfwGetKey(game->window, GLFW_KEY_DOWN) == GLFW_PRESS) {
        game->self->position.z += speed;
        game->player_moved = true;
    } else if (glfwGetKey(game->window, GLFW_KEY_LEFT) == GLFW_PRESS) {
        game->self->position.x -= speed;
        game->player_moved = true;
    } else if (glfwGetKey(game->window, GLFW_KEY_RIGHT) == GLFW_PRESS) {
        game->self->position.x += speed;
        game->player_moved = true;
    }

    PERSIST f32 acc_updt = 0.0f;
    acc_updt += dt;
    if (acc_updt >= game->client_update_period && game->player_moved) {
        game->player_moved = false;
        acc_updt = 0.0f;

        Packet_Player_Move packet = {};
        packet.id = game->self->id;
        memcpy(packet.position, glm::value_ptr(game->self->position), 3 * sizeof(f32));

        if (!packet_send(game->client_socket, PACKET_TYPE_PLAYER_MOVE, &packet)) {
            LOG_ERROR("failed to send player move packet\n");
        }
    }
}

// This module is hot-reloadable using dlopen
// We need to prevent function name mangling by the C++ compiler,
// in order to retrieve functions by name using dlsym
extern "C" {

void game_init(Game *game)
{
    game->current_window_width = WINDOW_WIDTH;
    game->current_window_height = WINDOW_HEIGHT;
    game->is_polygon_mode = false;
    game->camera_position = glm::vec3(0.0f, 0.0f, 3.0f);
    game->camera_direction = glm::vec3(0.0f, 0.0f, -1.0f);
    game->camera_up = glm::vec3(0.0f, 1.0f, 0.0f);
    game->camera_fov = 45.0f;
    game->camera_pitch = 0.0f;
    game->camera_yaw = -90.0f;
    game->vao = 0;
    game->vbo = 0;
    game->flat_color_shader = 0;
    game->lighting_shader = 0;

    u32 vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    u32 fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    u32 fragment_shader_lighting = create_shader(GL_FRAGMENT_SHADER, fragment_shader_lighting_source);
    game->flat_color_shader = create_program(vertex_shader, fragment_shader);
    game->lighting_shader = create_program(vertex_shader, fragment_shader_lighting);

    f32 vertices[] = {
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
        -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

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
         0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
         0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
        -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
         0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
        -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
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

    glEnable(GL_DEPTH_TEST);
}

void game_update(Game *game, f32 dt)
{
    glClearColor(0.192f, 0.192f, 0.5f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    process_input(game, dt);

    glm::mat4 projection = glm::perspective(glm::radians(game->camera_fov), (f32) game->current_window_width / (f32) game->current_window_height, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(game->camera_position, game->camera_position + game->camera_direction, game->camera_up);

    f32 light_speed = 1.0f;
    f32 radius = 5.0f;
    f32 now = (f32) glfwGetTime();
    glm::vec3 light_position = glm::vec3(glm::cos(now * light_speed) * radius, 3.0f, glm::sin(now * light_speed) * radius);

    glUseProgram(game->lighting_shader);
    glUniformMatrix4fv(glGetUniformLocation(game->lighting_shader, "projection"), 1, false, glm::value_ptr(projection));
    glUniformMatrix4fv(glGetUniformLocation(game->lighting_shader, "view"), 1, false, glm::value_ptr(view));
    glUniform3fv(glGetUniformLocation(game->lighting_shader, "camera_pos"), 1, glm::value_ptr(game->camera_position));
    glUniform3fv(glGetUniformLocation(game->lighting_shader, "light.position"), 1, glm::value_ptr(light_position));
    glUniform3f(glGetUniformLocation(game->lighting_shader, "light.ambient"), 0.2f, 0.2f, 0.2f);
    glUniform3f(glGetUniformLocation(game->lighting_shader, "light.diffuse"), 0.5f, 0.5f, 0.5f);
    glUniform3f(glGetUniformLocation(game->lighting_shader, "light.specular"), 1.0f, 1.0f, 1.0f);
    glUniform3f(glGetUniformLocation(game->lighting_shader, "mat.specular"), 0.5f, 0.5f, 0.5f);
    glUniform1f(glGetUniformLocation(game->lighting_shader, "mat.shininess"), 32.0f);
    glBindVertexArray(game->vao);

    // Render players
    for (Player *player = game->players; player != NULL; player = (Player *) player->hh.next) {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), player->position);
        glUniformMatrix4fv(glGetUniformLocation(game->lighting_shader, "model"), 1, false, glm::value_ptr(model));
        glUniform3fv(glGetUniformLocation(game->lighting_shader, "mat.ambient"), 1, glm::value_ptr(player->color));
        glUniform3fv(glGetUniformLocation(game->lighting_shader, "mat.diffuse"), 1, glm::value_ptr(player->color));
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    {
        // Render ourselves
        glm::mat4 model = glm::translate(glm::mat4(1.0f), game->self->position);
        glUniformMatrix4fv(glGetUniformLocation(game->lighting_shader, "model"), 1, false, glm::value_ptr(model));
        glUniform3fv(glGetUniformLocation(game->lighting_shader, "mat.ambient"), 1, glm::value_ptr(game->self->color));
        glUniform3fv(glGetUniformLocation(game->lighting_shader, "mat.diffuse"), 1, glm::value_ptr(game->self->color));
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    {
        // Render light
        glUseProgram(game->flat_color_shader);

        glm::mat4 light_model = glm::translate(glm::mat4(1.0f), light_position) * glm::scale(glm::mat4(1.0f), glm::vec3(0.25f));
        glUniformMatrix4fv(glGetUniformLocation(game->flat_color_shader, "projection"), 1, false, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(game->flat_color_shader, "view"), 1, false, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(game->flat_color_shader, "model"), 1, false, glm::value_ptr(light_model));
        glUniform4f(glGetUniformLocation(game->flat_color_shader, "color"), 0.9f, 0.9f, 0.9f, 1.0f);

        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    glfwSwapBuffers(game->window);
    glfwPollEvents();
}

void game_shutdown(Game *game)
{
    glDeleteProgram(game->flat_color_shader);
    glDeleteProgram(game->lighting_shader);
    glDeleteVertexArrays(1, &game->vao);
    glDeleteBuffers(1, &game->vbo);
}

} // extern "C"
