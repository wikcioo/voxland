#include "command_manager.h"

#include <string.h>

#include "common/log.h"
#include "common/asserts.h"
#include "common/memory/memutils.h"

Console_Command *cmds = NULL; // uthash

char *shift(u32 *argc, char ***argv)
{
    ASSERT(*argc > 0);
    char *result = **argv;
    *argc -= 1;
    *argv += 1;
    return result;
}

bool command_manager_run(const char *name, u32 argc, char **argv)
{
    Console_Command *cmd;
    HASH_FIND_STR(cmds, name, cmd);
    if (cmd == NULL) {
        LOG_ERROR("unknown console command `%s`\n", name);
        return false;
    }

    if (argc >= 1 && (strcmp(argv[0], "-h") == 0 || strcmp(argv[0], "--help") == 0)) {
        LOG_INFO("description: %s\n", cmd->description);
        if (cmd->usage == NULL) {
            LOG_INFO("no usage for `%s`\n", name);
        } else {
            cmd->usage();
        }
        return true;
    }

    return cmd->handler(argc, argv);
}

void command_manager_register(Console_Command cmd)
{
    Console_Command *temp;
    HASH_FIND_STR(cmds, cmd.name, temp);
    if (temp != NULL) {
        LOG_ERROR("console command `%s` already registered\n", cmd.name);
        return;
    }

    Console_Command *new_cmd = (Console_Command *) mem_alloc(sizeof(Console_Command), MEMORY_TAG_CONSOLE);
    mem_copy(new_cmd->name, cmd.name, strlen(cmd.name));
    mem_copy(new_cmd->description, cmd.description, strlen(cmd.description));
    new_cmd->usage = cmd.usage;
    new_cmd->handler = cmd.handler;
    HASH_ADD_STR(cmds, name, new_cmd);
}

void command_manager_cleanup(void)
{
    Console_Command *cmd, *tmp;
    HASH_ITER(hh, cmds, cmd, tmp) {
        HASH_DEL(cmds, cmd);
        mem_free(cmd, sizeof(Console_Command), MEMORY_TAG_CONSOLE);
    }
}
