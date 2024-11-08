#include "commands.h"

#include <string.h>

#include <GLFW/glfw3.h>

#include "client.h"
#include "command_manager.h"
#include "common/log.h"
#include "common/memory/memutils.h"

// Used by cmd_help to automatically retrieve all available commands.
extern Console_Command *cmds;

LOCAL bool cmd_help(u32 argc, char **argv)
{
    UNUSED(argc); UNUSED(argv);

    LOG_INFO("available commands:\n");
    for (Console_Command *cmd = cmds; cmd != NULL; cmd = (Console_Command *) cmd->hh.next) {
        LOG_INFO("  %s: %s\n", cmd->name, cmd->description);
    }

    return true;
}

void cmd_register_all(void)
{
    Console_Command cmd = {};

    strncpy(cmd.name, "reload", CONSOLE_CMD_MAX_NAME_LEN);
    strncpy(cmd.description, "reload libgame shared object", CONSOLE_CMD_MAX_DESCRIPTION_LEN);
    cmd.usage = cmd_reload_usage;
    cmd.handler = cmd_reload;
    command_manager_register(cmd);

    strncpy(cmd.name, "vsync", CONSOLE_CMD_MAX_NAME_LEN);
    strncpy(cmd.description, "turn on/off vsync on the main window", CONSOLE_CMD_MAX_DESCRIPTION_LEN);
    cmd.usage = cmd_vsync_usage;
    cmd.handler = cmd_vsync;
    command_manager_register(cmd);

    strncpy(cmd.name, "showinfo", CONSOLE_CMD_MAX_NAME_LEN);
    strncpy(cmd.description, "manage which information is displayed on the screen", CONSOLE_CMD_MAX_DESCRIPTION_LEN);
    cmd.usage = cmd_showinfo_usage;
    cmd.handler = cmd_showinfo;
    command_manager_register(cmd);

    // NOTE: Always keep cmd_help as the last registered command.
    strncpy(cmd.name, "help", CONSOLE_CMD_MAX_NAME_LEN);
    strncpy(cmd.description, "display available commands", CONSOLE_CMD_MAX_DESCRIPTION_LEN);
    cmd.usage = NULL;
    cmd.handler = cmd_help;
    command_manager_register(cmd);
}

void cmd_reload_usage(void)
{
    LOG_INFO("usage: reload\n");
}

bool cmd_reload(u32 argc, char **argv)
{
    UNUSED(argc); UNUSED(argv);

    if (reload_libgame()) {
        LOG_INFO("successfully reloaded libgame\n");
        return true;
    }

    LOG_ERROR("failed to reload libgame\n");
    return false;
}

void cmd_vsync_usage(void)
{
    LOG_INFO("usage: vsync <state>\n");
    LOG_INFO("  <state> allowed values: on, off\n");
}

bool cmd_vsync(u32 argc, char **argv)
{
    if (argc == 0) {
        cmd_vsync_usage();
        LOG_ERROR("missing argument\n");
        return false;
    }

    const char *state = shift(&argc, &argv);
    if (strcmp(state, "on") == 0) {
        glfwSwapInterval(1);
        LOG_INFO("turned on vsync\n");
        return true;
    } else if (strcmp(state, "off") == 0) {
        glfwSwapInterval(0);
        LOG_INFO("turned off vsync\n");
        return true;
    }

    LOG_ERROR("unknown vsync argument `%s`\n", state);
    return false;
}

void cmd_showinfo_usage(void)
{
    LOG_INFO("usage: showinfo <type> <state>\n");
    LOG_INFO("  <type> allowed values: fps, network, memory_usage\n");
    LOG_INFO("  <state> allowed values: on, off\n");
}

bool cmd_showinfo(u32 argc, char **argv)
{
    if (argc < 2) {
        cmd_showinfo_usage();
        LOG_ERROR("missing argument(s)\n");
        return false;
    }

    const char *type = shift(&argc, &argv);
    const char *state = shift(&argc, &argv);

    bool b_state;
    if (strcmp(state, "on") == 0) {
        b_state = true;
    } else if (strcmp(state, "off") == 0) {
        b_state = false;
    } else {
        LOG_ERROR("unknown showinfo argument `%s`\n", state);
        return false;
    }

    if (strcmp(type, "fps") == 0) {
        client_show_fps_info = b_state;
    } else if (strcmp(type, "network") == 0) {
        client_show_net_info = b_state;
    } else if (strcmp(type, "memory_usage") == 0) {
        client_show_mem_info = b_state;
    } else {
        LOG_ERROR("unknown showinfo argument `%s`\n", type);
        return false;
    }

    return true;
}
