#include "net.h"

#include <sys/socket.h>

#include "asserts.h"

LOCAL Net_Stat *net_stat;

void net_init(Net_Stat *ns)
{
    net_stat = ns;
}

i64 net_send(i32 socket, const void *buffer, u64 size, i32 flags)
{
    ASSERT_MSG(net_stat, "net_send: net_stat not initialized");

    i64 bytes_sent = send(socket, buffer, size, flags);
    if (bytes_sent > 0) {
        net_stat->bpp_up += bytes_sent;
    }

    return bytes_sent;
}

i64 net_recv(i32 socket, void *buffer, u64 size, i32 flags)
{
    ASSERT_MSG(net_stat, "net_recv: net_stat not initialized");

    i64 bytes_read = recv(socket, buffer, size, flags);
    if (bytes_read > 0) {
        net_stat->bpp_down += bytes_read;
    }

    return bytes_read;
}

void net_get_bandwidth(u64 *up, u64 *down)
{
    ASSERT_MSG(net_stat, "net_get_bandwidth: net_stat not initialized");

    *up = net_stat->last_bpp_up;
    *down = net_stat->last_bpp_down;
}

void net_update(f32 dt)
{
    ASSERT_MSG(net_stat, "net_update: net_stat not initialized");

    net_stat->acc += dt;
    if (net_stat->acc >= NET_STAT_UPDATE_PERIOD) {
        net_stat->last_bpp_up = net_stat->bpp_up;
        net_stat->last_bpp_down = net_stat->bpp_down;
        net_stat->bpp_up = 0;
        net_stat->bpp_down = 0;
        net_stat->acc = 0.0f;
    }
}
