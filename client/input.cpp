#include "input.h"

#include <GLFW/glfw3.h>

#include "window.h"

bool input_is_key_pressed(Keycode key)
{
    i32 state = glfwGetKey((GLFWwindow *) window_get_native_window(), key);
    return state == INPUTACTION_Press || state == INPUTACTION_Repeat;
}

bool input_is_mouse_button_pressed(Mousebutton button)
{
    i32 state = glfwGetMouseButton((GLFWwindow *) window_get_native_window(), button);
    return state == INPUTACTION_Press;
}

glm::vec2 input_get_mouse_position(void)
{
    f64 xpos, ypos;
    glfwGetCursorPos((GLFWwindow *) window_get_native_window(), &xpos, &ypos);
    return glm::vec2(xpos, ypos);
}
