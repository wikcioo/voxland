#include "window.h"

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "input_codes.h"
#include "common/log.h"
#include "common/event.h"

LOCAL GLFWwindow *main_window;
LOCAL glm::ivec2 main_window_size;

LOCAL void glfw_error_callback(i32 code, const char *description)
{
    LOG_ERROR("glfw error (%d): %s\n", code, description);
}

LOCAL void glfw_key_callback(GLFWwindow *window, i32 key, i32 scancode, i32 action, i32 mods)
{
    UNUSED(window); UNUSED(scancode);

    Event_Data data = {};
    data.U16[0] = (u16) key;
    data.U16[1] = (u16) mods;

    if (action == INPUTACTION_Press) {
        event_system_fire(EVENT_CODE_KEY_PRESSED, data);
    } else if (action == INPUTACTION_Release) {
        event_system_fire(EVENT_CODE_KEY_RELEASED, data);
    } else if (action == INPUTACTION_Repeat) {
        event_system_fire(EVENT_CODE_KEY_REPEATED, data);
    }
}

LOCAL void glfw_char_callback(GLFWwindow *window, u32 codepoint)
{
    UNUSED(window);

    Event_Data data = {};
    data.U32[0] = codepoint;
    event_system_fire(EVENT_CODE_CHAR_PRESSED, data);
}

LOCAL void glfw_mouse_moved_callback(GLFWwindow *window, f64 xpos, f64 ypos)
{
    UNUSED(window);

    Event_Data data = {};
    data.F32[0] = (f32) xpos;
    data.F32[1] = (f32) ypos;
    event_system_fire(EVENT_CODE_MOUSE_MOVED, data);
}

LOCAL void glfw_framebuffer_size_callback(GLFWwindow *window, i32 width, i32 height)
{
    UNUSED(window);

    main_window_size.x = width;
    main_window_size.y = height;
    glViewport(0, 0, width, height);

    Event_Data data = {};
    data.U32[0] = width;
    data.U32[1] = height;
    event_system_fire(EVENT_CODE_WINDOW_RESIZED, data);
}

LOCAL void glfw_window_close_callback(GLFWwindow *window)
{
    UNUSED(window);
    event_system_fire(EVENT_CODE_WINDOW_CLOSED, {});
}

bool window_create(u32 width, u32 height, const char *title)
{
    glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit()) {
        LOG_FATAL("failed to initialize glfw\n");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    main_window = glfwCreateWindow(width, height, title, NULL, NULL);
    if (main_window == NULL) {
        LOG_FATAL("failed to create glfw window\n");
        glfwTerminate();
        return false;
    }

    main_window_size = glm::vec2(width, height);

    glfwMakeContextCurrent(main_window);

    glfwSetKeyCallback(main_window, glfw_key_callback);
    glfwSetCharCallback(main_window, glfw_char_callback);
    glfwSetCursorPosCallback(main_window, glfw_mouse_moved_callback);
    glfwSetFramebufferSizeCallback(main_window, glfw_framebuffer_size_callback);
    glfwSetWindowCloseCallback(main_window, glfw_window_close_callback);

    glfwSwapInterval(VSYNC_ENABLED);

    u32 err = glewInit();
    if (err != GLEW_OK) {
        LOG_FATAL("failed to initialize glew: %s\n", glewGetErrorString(err));
        glfwDestroyWindow(main_window);
        glfwTerminate();
        return false;
    }

    const GLFWvidmode *vidmode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    LOG_INFO("primary monitor parameters:\n");
    LOG_INFO("  width: %d\n", vidmode->width);
    LOG_INFO("  height: %d\n", vidmode->height);
    LOG_INFO("  refresh rate: %d\n", vidmode->refreshRate);

    LOG_INFO("graphics info:\n");
    LOG_INFO("  vendor: %s\n", glGetString(GL_VENDOR));
    LOG_INFO("  renderer: %s\n", glGetString(GL_RENDERER));
    LOG_INFO("  opengl version: %s\n", glGetString(GL_VERSION));
    LOG_INFO("  glsl version: %s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    LOG_INFO("running glfw version %s\n", glfwGetVersionString());
    LOG_INFO("vsync: %s\n", VSYNC_ENABLED ? "on" : "off");

    return true;
}

void window_destroy(void)
{
    glfwDestroyWindow(main_window);
    glfwTerminate();
    LOG_INFO("window destroyed successfully\n");
}

void window_poll_events(void)
{
    glfwPollEvents();
}

void window_swap_buffers(void)
{
    glfwSwapBuffers(main_window);
}

glm::vec2 window_get_size(void)
{
    return main_window_size;
}

void *window_get_native_window(void)
{
    return main_window;
}
