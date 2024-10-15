#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define WINDOW_WIDTH 640
#define WINDOW_HEIGHT 480

#define UNUSED(x) ((void) x)

const char *vertex_shader_source =
    "#version 330 core\n"
    "layout(location = 0) in vec3 a_position;\n"
    "uniform mat4 projection;\n"
    "uniform mat4 view;\n"
    "uniform mat4 model;\n"
    "void main() {\n"
    "    gl_Position = projection * view * model * vec4(a_position, 1.0);\n"
    "}\n";

const char *fragment_shader_source =
    "#version 330 core\n"
    "out vec4 out_color;\n"
    "uniform vec4 color;\n"
    "void main() {\n"
    "    out_color = color;\n"
    "}\n";

glm::vec3 camera_position  = glm::vec3(0.0f, 0.0f, 3.0f);
glm::vec3 camera_direction = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 camera_up        = glm::vec3(0.0f, 1.0f, 0.0f);
float     camera_fov       = 45.0f;
float     camera_pitch     = 0.0f;
float     camera_yaw       = -90.0f;

uint32_t current_window_width = WINDOW_WIDTH;
uint32_t current_window_height = WINDOW_HEIGHT;

static void glfw_error_callback(int code, const char *description)
{
    fprintf(stderr, "error: glfw error (%d): %s\n", code, description);
}

static void glfw_framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    UNUSED(window);
    glViewport(0, 0, width, height);
    current_window_width = width;
    current_window_height = height;
}

static bool is_polygon_mode = false;
static void glfw_key_callback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    UNUSED(window);
    UNUSED(scancode);
    UNUSED(mods);

    if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        is_polygon_mode = !is_polygon_mode;
        int32_t mode = is_polygon_mode ? GL_LINE : GL_FILL;
        glPolygonMode(GL_FRONT_AND_BACK, mode);
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

    camera_yaw += xoffset;
    camera_pitch += yoffset;

    if (camera_pitch > 89.0f) {
        camera_pitch = 89.0f;
    } else if (camera_pitch < -89.0f) {
        camera_pitch = -89.0f;
    }

    glm::vec3 direction;
    direction.x = glm::cos(glm::radians(camera_yaw)) * glm::cos(glm::radians(camera_pitch));
    direction.y = glm::sin(glm::radians(camera_pitch));
    direction.z = glm::sin(glm::radians(camera_yaw)) * glm::cos(glm::radians(camera_pitch));
    camera_direction = glm::normalize(direction);
}

uint32_t create_shader(uint32_t type, const char *source)
{
    uint32_t shader = glCreateShader(type);
    glShaderSource(shader, 1, (const GLchar **)&source, 0);
    glCompileShader(shader);

    int is_compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &is_compiled);
    if (is_compiled == false) {
        char info_log[1024] = {0};
        glGetShaderInfoLog(shader, 1024, 0, info_log);
        const char *shader_type_as_cstr = type == GL_VERTEX_SHADER ? "vertex" : (type == GL_FRAGMENT_SHADER ? "fragment" : "unknown");
        fprintf(stderr, "error: failed to compile %s shader: %s\n", shader_type_as_cstr, info_log);
        exit(1);
    }

    return shader;
}

uint32_t create_program(uint32_t vertex_shader, uint32_t fragment_shader)
{
    uint32_t program = glCreateProgram();

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    int is_linked;
    glGetProgramiv(program, GL_LINK_STATUS, &is_linked);
    if (is_linked == false) {
        char info_log[1024];
        glGetProgramInfoLog(program, 1024, 0, info_log);
        fprintf(stderr, "error: failed to link program: %s\n", info_log);
        exit(1);
    }

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    return program;
}

void process_input(GLFWwindow *window, float delta_time)
{
    static float velocity = 5.0f;
    float speed = delta_time * velocity;

    glm::vec3 camera_right = glm::normalize(glm::cross(camera_direction, camera_up));

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        camera_position += camera_direction * speed;
    } else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        camera_position -= camera_direction * speed;
    } else if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        camera_position -= camera_right * speed;
    } else if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        camera_position += camera_right * speed;
    }
}

int main(void)
{
    printf("Hello, Voxland Client\n");

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

    GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "voxland client", NULL, NULL);
    if (!window) {
        fprintf(stderr, "error: failed to create glfw window\n");
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, glfw_framebuffer_size_callback);
    glfwSetKeyCallback(window, glfw_key_callback);
    glfwSetCursorPosCallback(window, glfw_mouse_moved_callback);

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    uint32_t err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "error: failed to initialize glew: %s\n", glewGetErrorString(err));
        glfwDestroyWindow(window);
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

    glEnable(GL_DEPTH_TEST);

    uint32_t vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    uint32_t fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    uint32_t shader = create_program(vertex_shader, fragment_shader);

    float vertices[] = {
        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,

        -0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,

        -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,

         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,

        -0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f,

        -0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f,
    };

    uint32_t vao, vbo;

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (const void *) 0);
    glEnableVertexAttribArray(0);

    float delta_time = 0.0;
    float last_time = 0.0;

    while (!glfwWindowShouldClose(window)) {
        float now = (float) glfwGetTime();
        delta_time = now - last_time;
        last_time = now;

        process_input(window, delta_time);
        glm::mat4 projection = glm::perspective(glm::radians(camera_fov), (float) current_window_width / (float) current_window_height, 0.1f, 100.0f);
        glm::mat4 view = glm::lookAt(camera_position, camera_position + camera_direction, camera_up);
        glm::mat4 model = glm::mat4(1.0f);

        glClearColor(0.384f, 0.384f, 1.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shader);

        // vertex shader uniforms
        glUniformMatrix4fv(glGetUniformLocation(shader, "projection"), 1, false, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shader, "view"), 1, false, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shader, "model"), 1, false, glm::value_ptr(model));

        // fragment shader uniforms
        glUniform4f(glGetUniformLocation(shader, "color"), 0.8f, 0.5f, 0.2f, 1.0f);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteProgram(shader);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
