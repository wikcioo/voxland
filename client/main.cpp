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

static void glfw_error_callback(int code, const char *description)
{
    fprintf(stderr, "error: glfw error (%d): %s\n", code, description);
}

static void glfw_framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    UNUSED(window);
    glViewport(0, 0, width, height);
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

    glm::vec3 camera_position = glm::vec3(1.0f, 2.0f, 3.0f);
    glm::vec3 camera_direction = glm::normalize(glm::vec3(-1.0f, -2.0f, -3.0f));
    glm::vec3 camera_up = glm::vec3(0.0f, 1.0f, 0.0f);
    float fov = 45.0f;

    glm::mat4 projection = glm::perspective(glm::radians(fov), (float) WINDOW_WIDTH / (float) WINDOW_HEIGHT, 0.1f, 100.0f);
    glm::mat4 view = glm::lookAt(camera_position, camera_position + camera_direction, camera_up);
    glm::mat4 model = glm::mat4(1.0f);

    while (!glfwWindowShouldClose(window)) {
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
