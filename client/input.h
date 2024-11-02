#pragma once

#include <glm/glm.hpp>

#include "input_codes.h"
#include "common/defines.h"

bool input_is_key_pressed(Keycode key);
bool input_is_mouse_button_pressed(Mousebutton button);
glm::vec2 input_get_mouse_position(void);
