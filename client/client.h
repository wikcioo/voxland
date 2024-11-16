#pragma once

#define CLIENT_JOB_SYSTEM_NUM_WORKERS 4

extern bool client_show_fps_info;
extern bool client_show_net_info;
extern bool client_show_mem_info;

bool reload_libgame(void);
void ping_server(void);
