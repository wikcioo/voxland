#pragma once

#include <glm/glm.hpp>

#include "common/defines.h"

#define VSYNC_ENABLED 1

bool window_create(u32 width, u32 height, const char *title);
void window_destroy(void);

void window_poll_events(void);
void window_swap_buffers(void);

glm::vec2 window_get_size(void);
void *window_get_native_window(void);
