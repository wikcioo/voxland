#pragma once

#include "defines.h"

#define NET_STAT_UPDATE_PERIOD 0.5f

typedef struct {
    f32 acc;
    u64 bpp_up; // bytes per period
    u64 bpp_down;
    u64 last_bpp_up;
    u64 last_bpp_down;
} Net_Stat;

void net_init(Net_Stat *ns);
i64 net_send(i32 socket, const void *buffer, u64 size, i32 flags);
i64 net_recv(i32 socket, void *buffer, u64 size, i32 flags);
void net_get_bandwidth(u64 *up, u64 *down);
void net_update(f32 dt);
