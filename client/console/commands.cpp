#include "commands.h"

#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

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

    strncpy(cmd.name, "skybox", CONSOLE_CMD_MAX_NAME_LEN);
    strncpy(cmd.description, "turn on/off skybox rendering", CONSOLE_CMD_MAX_DESCRIPTION_LEN);
    cmd.usage = cmd_skybox_usage;
    cmd.handler = cmd_skybox;
    command_manager_register(cmd);

    strncpy(cmd.name, "ping", CONSOLE_CMD_MAX_NAME_LEN);
    strncpy(cmd.description, "send ping packets to the server", CONSOLE_CMD_MAX_DESCRIPTION_LEN);
    cmd.usage = cmd_ping_usage;
    cmd.handler = cmd_ping;
    command_manager_register(cmd);

    strncpy(cmd.name, "wireframe", CONSOLE_CMD_MAX_NAME_LEN);
    strncpy(cmd.description, "turn on/off wireframe mode for rendering", CONSOLE_CMD_MAX_DESCRIPTION_LEN);
    cmd.usage = cmd_wireframe_usage;
    cmd.handler = cmd_wireframe;
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

void cmd_skybox_usage(void)
{
    LOG_INFO("usage: skybox <state>\n");
    LOG_INFO("  <state> allowed values: on, off\n");
}

bool cmd_skybox(u32 argc, char **argv)
{
    if (argc == 0) {
        cmd_skybox_usage();
        LOG_ERROR("missing argument\n");
        return false;
    }

    const char *state = shift(&argc, &argv);
    if (strcmp(state, "on") == 0) {
        global_data.skybox_visible = true;
        LOG_INFO("turned on skybox rendering\n");
        return true;
    } else if (strcmp(state, "off") == 0) {
        global_data.skybox_visible = false;
        LOG_INFO("turned off skybox rendering\n");
        return true;
    }

    LOG_ERROR("unknown skybox argument `%s`\n", state);
    return false;
}

typedef struct {
    u32 count;
    f32 interval;
} Ping_Conf;

LOCAL bool ping_thread_running = false;
LOCAL pthread_t ping_thread;

LOCAL void *run_ping(void *arg)
{
    ping_thread_running = true;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

    Ping_Conf *conf = (Ping_Conf *) arg;
    u32 count = conf->count;
    u32 interval_us = (u32) (conf->interval * 1e6);

    for (u32 i = 0; i < count; i++) {
        ping_server();
        usleep(interval_us);
    }

    ping_thread_running = false;
    return NULL;
}

LOCAL bool init_ping_task(Ping_Conf conf)
{
    if (ping_thread_running) {
        LOG_ERROR("ping already running\n");
        return false;
    }

    pthread_attr_t attr;
    i32 ret;

    ret = pthread_attr_init(&attr);
    if (ret != 0) {
        LOG_ERROR("failed to initialize thread attributes\n");
        return false;
    }

    ret = pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (ret != 0) {
        LOG_ERROR("failed to set detached state for thread\n");
        goto _cleanup;
    }

    ret = pthread_create(&ping_thread, &attr, run_ping, &conf);
    if (ret != 0) {
        LOG_ERROR("failed to create thread\n");
        goto _cleanup;
    }

_cleanup:
    pthread_attr_destroy(&attr);
    return ret == 0;
}

void cmd_ping_usage(void)
{
    LOG_INFO("usage: ping [flags]\n");
    LOG_INFO("  [flags] allowed values:\n");
    LOG_INFO("    --stop: stop sending ping packets\n");
    LOG_INFO("    -c <count:u32>: send <count> number of ping packets\n");
    LOG_INFO("    -i <interval:f32>: wait <interval> seconds between sending each ping packet\n");
}

// TODO: Display statistics at the end of sending all ping packets (e.g. average response time)
bool cmd_ping(u32 argc, char **argv)
{
    Ping_Conf conf = {
        .count = UINT32_MAX,
        .interval = 1.0f
    };

    if (argc == 0) {
        return init_ping_task(conf);
    }

    while (argc > 0) {
        const char *flag = shift(&argc, &argv);

        if (strcmp(flag, "--stop") == 0) {
            if (!ping_thread_running) {
                LOG_INFO("nothing to stop\n");
                return true;
            }

            if (pthread_cancel(ping_thread) != 0) {
                LOG_ERROR("failed to cancel ping thread\n");
                return false;
            }

            ping_thread_running = false;
            LOG_INFO("stopped\n");
            return true;
        } else if (strcmp(flag, "-c") == 0) {
            if (argc == 0) {
                LOG_ERROR("missing argument for flag `%s`\n", flag);
                return false;
            }

            const char *count_as_cstr = shift(&argc, &argv);

            char *end_ptr;
            errno = 0;
            u64 count = strtoul(count_as_cstr, &end_ptr, 10);

            if (errno == ERANGE || count > UINT32_MAX) {
                LOG_ERROR("count value `%s` out of range of u32\n", count_as_cstr);
                return false;
            } else if (end_ptr == count_as_cstr) {
                LOG_ERROR("could not convert `%s` to a valid count\n", count_as_cstr);
                return false;
            } else if (*end_ptr != '\0') {
                LOG_ERROR("trailing characters detected in `%s`\n", count_as_cstr);
                return false;
            }

            conf.count = (u32) count;
        } else if (strcmp(flag, "-i") == 0) {
            if (argc == 0) {
                LOG_ERROR("missing argument for flag `%s`\n", flag);
                return false;
            }

            const char *interval_as_cstr = shift(&argc, &argv);

            char *end_ptr;
            errno = 0;
            f32 interval = strtof(interval_as_cstr, &end_ptr);

            if (end_ptr == interval_as_cstr) {
                LOG_ERROR("could not convert `%s` to a valid interval (must be a floating point number)\n", interval_as_cstr);
                return false;
            } else if (errno == ERANGE) {
                if (interval == HUGE_VALF) {
                    LOG_ERROR("interval value `%s` overflows\n", interval_as_cstr);
                } else if (interval == 0.0f) {
                    LOG_ERROR("interval value `%s` underflows\n", interval_as_cstr);
                }
                return false;
            } else if (*end_ptr != '\0') {
                LOG_ERROR("trailing characters detected in `%s`\n", interval_as_cstr);
                return false;
            } else if (interval <= 0.0f) {
                LOG_ERROR("interval must be greater than 0.0 seconds\n");
                return false;
            }

            conf.interval = interval;
        } else {
            LOG_ERROR("unknown flag `%s`\n", flag);
            return false;
        }
    }

    return init_ping_task(conf);
}

void cmd_wireframe_usage(void)
{
    LOG_INFO("usage: wireframe <state>\n");
    LOG_INFO("  <state> allowed values: on, off\n");
}

bool cmd_wireframe(u32 argc, char **argv)
{
    if (argc == 0) {
        cmd_wireframe_usage();
        LOG_ERROR("missing argument\n");
        return false;
    }

    const char *state = shift(&argc, &argv);
    if (strcmp(state, "on") == 0) {
        global_data.is_polygon_mode = true;
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        LOG_INFO("turned on wireframe mode\n");
        return true;
    } else if (strcmp(state, "off") == 0) {
        global_data.is_polygon_mode = false;
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        LOG_INFO("turned off wireframe mode\n");
        return true;
    }

    LOG_ERROR("unknown wireframe argument `%s`\n", state);
    return false;
}
