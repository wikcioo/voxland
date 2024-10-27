#pragma once

#include <poll.h>

#include "common/defines.h"

#define POLL_INFINITE_TIMEOUT -1

typedef struct {
    struct pollfd *fds;
    u32 count;
    u32 capacity;
} Pollfd_Set;

void pollfd_set_init(u32 initial_capacity, Pollfd_Set *out_pfds);
void pollfd_set_shutdown(Pollfd_Set *pfds);
void pollfd_set_add(Pollfd_Set *pfds, i32 fd);
void pollfd_set_remove(Pollfd_Set *pfds, i32 fd);
