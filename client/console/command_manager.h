#pragma once

#include "common/defines.h"
#include "uthash/uthash.h"

#define CONSOLE_CMD_MAX_NAME_LEN 31
#define CONSOLE_CMD_MAX_DESCRIPTION_LEN 127

typedef void (*pfn_cmd_usage)(void);
typedef bool (*pfn_cmd_handler)(u32 argc, char **argv);

typedef struct {
    char name[CONSOLE_CMD_MAX_NAME_LEN+1];
    char description[CONSOLE_CMD_MAX_DESCRIPTION_LEN+1];
    pfn_cmd_usage usage;
    pfn_cmd_handler handler;
    UT_hash_handle hh;
} Console_Command;

char *shift(u32 *argc, char ***argv);
bool command_manager_run(const char *name, u32 argc, char **argv);
void command_manager_register(Console_Command cmd);
void command_manager_cleanup(void);
