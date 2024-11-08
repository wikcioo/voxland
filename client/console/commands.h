#pragma once

#include "common/defines.h"

void cmd_register_all(void);

void cmd_reload_usage(void);
bool cmd_reload(u32 argc, char **argv);
void cmd_vsync_usage(void);
bool cmd_vsync(u32 argc, char **argv);
void cmd_showinfo_usage(void);
bool cmd_showinfo(u32 argc, char **argv);
