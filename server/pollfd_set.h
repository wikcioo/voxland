#pragma once

#include <poll.h>

#include "defines.h"

#define POLL_INFINITE_TIMEOUT -1

typedef struct {
    struct pollfd *fds;
    u32 count;
    u32 capacity;
} pollfd_set_t;

void pollfd_set_init(u32 initial_capacity, pollfd_set_t *out_pfds);
void pollfd_set_shutdown(pollfd_set_t *pfds);
void pollfd_set_add(pollfd_set_t *pfds, i32 fd);
void pollfd_set_remove(pollfd_set_t *pfds, i32 fd);
