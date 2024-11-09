#include "commands.h"

#include <errno.h>
#include <string.h>

#include <GLFW/glfw3.h>

#include "global.h"
#include "client.h"
#include "command_manager.h"
#include "common/log.h"
#include "common/size_unit.h"
#include "common/memory/memutils.h"
#include "common/collections/darray.h"

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

    strncpy(cmd.name, "log_registry", CONSOLE_CMD_MAX_NAME_LEN);
    strncpy(cmd.description, "get statistics of log registry", CONSOLE_CMD_MAX_DESCRIPTION_LEN);
    cmd.usage = cmd_log_registry_usage;
    cmd.handler = cmd_log_registry;
    command_manager_register(cmd);

    strncpy(cmd.name, "camera", CONSOLE_CMD_MAX_NAME_LEN);
    strncpy(cmd.description, "manage/retrieve camera related settings", CONSOLE_CMD_MAX_DESCRIPTION_LEN);
    cmd.usage = cmd_camera_usage;
    cmd.handler = cmd_camera;
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

void cmd_log_registry_usage(void)
{
    LOG_INFO("usage: log_registry\n");
}

bool cmd_log_registry(u32 argc, char **argv)
{
    UNUSED(argc); UNUSED(argv);

    u64 num_logs = darray_length(global_data.lr->logs);
    void *logs_memory = global_data.lr->allocator.memory;
    u64 total_size_in_bytes = global_data.lr->allocator.total_size;
    u64 used_size_in_bytes = global_data.lr->allocator.current_offset;

    f32 total_formatted = 0.0f, used_formatted = 0.0f, remaining_formatted = 0.0f;
    const char *total_unit = get_size_unit(total_size_in_bytes, &total_formatted);
    const char *used_unit = get_size_unit(used_size_in_bytes, &used_formatted);
    const char *remaining_unit = get_size_unit(total_size_in_bytes - used_size_in_bytes, &remaining_formatted);

    LOG_INFO("log registry statistics:\n");
    LOG_INFO("  number of log entries: %llu\n", num_logs);
    LOG_INFO("  memory:\n");
    LOG_INFO("    address: %p\n", logs_memory);
    LOG_INFO("    total size: %.2f %s\n", total_formatted, total_unit);
    LOG_INFO("    used size: %.2f %s\n", used_formatted, used_unit);
    LOG_INFO("    remaining size: %.2f %s\n", remaining_formatted, remaining_unit);

    return true;
}

void cmd_camera_usage(void)
{
    LOG_INFO("usage: camera [flags]\n");
    LOG_INFO("  [flags] allowed values:\n");
    LOG_INFO("    --speed-factor <factor:float>: set camera's speed factor to <factor>\n");
    LOG_INFO("    --get-properties: display camera's properties\n");
    LOG_INFO("    --reset-position: move camera to origin\n");
}

bool cmd_camera(u32 argc, char **argv)
{
    if (argc == 0) {
        cmd_camera_usage();
        LOG_INFO("no action taken\n");
        return true;
    }

    while (argc > 0) {
        const char *flag = shift(&argc, &argv);

        if (strcmp(flag, "--speed-factor") == 0) {
            if (argc == 0) {
                LOG_ERROR("missing argument for flag `%s`\n", flag);
                return false;
            }

            const char *factor_as_cstr = shift(&argc, &argv);

            char *end_ptr;
            errno = 0;
            f32 factor = strtof(factor_as_cstr, &end_ptr);

            if (end_ptr == factor_as_cstr) {
                LOG_ERROR("could not convert `%s` to a valid factor (must be a floating point number)\n", factor_as_cstr);
                return false;
            } else if (errno == ERANGE) {
                if (factor == HUGE_VALF) {
                    LOG_ERROR("factor value `%s` overflows\n", factor_as_cstr);
                } else if (factor == 0.0f) {
                    LOG_ERROR("factor value `%s` underflows\n", factor_as_cstr);
                }
                return false;
            } else if (*end_ptr != '\0') {
                LOG_ERROR("trailing characters detected in `%s`\n", factor_as_cstr);
                return false;
            }

            global_data.camera_speed_factor = factor;
            LOG_INFO("camera speed factor set to `%f`\n", global_data.camera_speed_factor);
        } else if (strcmp(flag, "--get-properties") == 0) {
            glm::vec3 camera_up = glm::normalize(global_data.camera_up);
            glm::vec3 camera_right = glm::normalize(glm::cross(global_data.camera_direction, global_data.camera_up));
            LOG_INFO("camera properties:\n");
            LOG_INFO("  position: { x: %f, y: %f, z: %f }\n", global_data.camera_position.x, global_data.camera_position.y, global_data.camera_position.z);
            LOG_INFO("  direction: { x: %f, y: %f, z: %f }\n", global_data.camera_direction.x, global_data.camera_direction.y, global_data.camera_direction.z);
            LOG_INFO("  up: { x: %f, y: %f, z: %f }\n", camera_up.x, camera_up.y, camera_up.z);
            LOG_INFO("  right: { x: %f, y: %f, z: %f }\n", camera_right.x, camera_right.y, camera_right.z);
            LOG_INFO("  fov: %f\n", global_data.camera_fov);
            LOG_INFO("  pitch: %f\n", global_data.camera_pitch);
            LOG_INFO("  yaw: %f\n", global_data.camera_yaw);
            LOG_INFO("  speed factor: %f\n", global_data.camera_speed_factor);
        } else if (strcmp(flag, "--reset-position") == 0) {
            global_data.camera_position = glm::vec3(0.0f);
            LOG_INFO("camera position set to { x: %f, y: %f, z: %f }\n", global_data.camera_position.x, global_data.camera_position.y, global_data.camera_position.z);
        } else {
            LOG_ERROR("unknown flag `%s`\n", flag);
            return false;
        }
    }

    return true;
}
