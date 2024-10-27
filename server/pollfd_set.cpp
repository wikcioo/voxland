#include "pollfd_set.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void pollfd_set_init(u32 initial_capacity, Pollfd_Set *out_pfds)
{
    out_pfds->count = 0;
    out_pfds->capacity = initial_capacity;

    u32 fds_size = sizeof(struct pollfd) * initial_capacity;
    out_pfds->fds = (struct pollfd *) malloc(fds_size);
    memset(out_pfds->fds, 0, fds_size);
}

void pollfd_set_shutdown(Pollfd_Set *pfds)
{
    pfds->count = 0;
    pfds->capacity = 0;
    free(pfds->fds);
}

void pollfd_set_add(Pollfd_Set *pfds, i32 fd)
{
    if (pfds->count + 1 >= pfds->capacity) {
        void *ptr = realloc((void *) pfds->fds, pfds->capacity * 2);
        if (ptr != NULL) {
            pfds->capacity = pfds->capacity * 2;
            pfds->fds = (struct pollfd *) ptr;
        } else {
            fprintf(stderr, "failed to resize pollfd array\n");
        }
    }

    pfds->fds[pfds->count].fd = fd;
    pfds->fds[pfds->count].events = POLLIN;
    pfds->count++;
}

void pollfd_set_remove(Pollfd_Set *pfds, i32 fd)
{
    for (u32 i = 0; i < pfds->count; i++) {
        if (pfds->fds[i].fd == fd) {
            pfds->fds[i].fd = pfds->fds[pfds->count-1].fd;
            pfds->count--;
            return;
        }
    }

    fprintf(stderr, "did not find fd=%d in the array of pfds\n", fd);
}
