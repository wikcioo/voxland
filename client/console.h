#pragma once

#include "event.h"
#include "renderer2d.h"
#include "common/defines.h"

void console_init(void);
void console_update(Renderer2D *renderer2d, const glm::mat4 *ui_projection, u32 ww, u32 wh, f32 dt);
bool console_on_key_pressed_event(Event_Code code, Event_Data data);
bool console_on_key_repeated_event(Event_Code code, Event_Data data);
bool console_on_char_pressed_event(Event_Code code, Event_Data data);
