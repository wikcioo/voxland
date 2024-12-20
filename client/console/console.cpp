#include "console.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

#include "client.h"
#include "input.h"
#include "input_codes.h"
#include "commands.h"
#include "command_manager.h"
#include "common/log.h"
#include "common/clock.h"
#include "common/asserts.h"
#include "common/filesystem.h"
#include "common/string_view.h"
#include "common/collections/darray.h"

#define CONSOLE_PROMPT_LEN 7
#define CONSOLE_PROMPT "input> "
#define CONSOLE_MAX_INPUT_SIZE 256
#define CONSOLE_ARGV_CAPACITY 32

#define CONSOLE_MAX_WIDTH 1000.0f
#define CONSOLE_PAD_X 10.0f
#define CONSOLE_LOG_PAD_X 5.0f
#define CONSOLE_LOG_PAD_Y 5.0f
#define CONSOLE_INPUT_PAD_X 5.0f
#define CONSOLE_INPUT_PAD_Y_RATIO_TO_FONT 1.25f

#define CONSOLE_CURSOR_BLINK_PERIOD 0.5f

#define COMMAND_HISTORY_FILEPATH "./.command_history"

typedef enum {
    CONSOLE_HIDDEN,
    CONSOLE_HALF_OPEN,
    CONSOLE_FULL_OPEN,
    NUM_OF_CONSOLE_STATES
} Console_State;

typedef enum {
    INPUT_NORMAL,
    INPUT_REVERSE_SEARCH
} Console_Input_State;

typedef struct {
    u32 length;
    char input[CONSOLE_MAX_INPUT_SIZE+1];
} Console_Cmd;

typedef struct {
    Console_State state;
    Console_Input_State input_state;
    bool is_changing_state;
    f32 target_height;
    f32 current_height;
    u32 cursor;
    bool cursor_visible;
    f32 cursor_blink_accumulator;
    char input[CONSOLE_MAX_INPUT_SIZE+1];
    bool reverse_search_failed;
    u32 reverse_search_result_length;
    u32 reverse_search_start_index;
    u32 reverse_search_result_index;
    char reverse_search_result[CONSOLE_MAX_INPUT_SIZE+1];
    i32 history_cursor;
    Console_Cmd *history; // darray
    Font_Atlas_Size font_size;
    char **logs; // darray
    File_Handle command_history_file_handle;
} Console;

LOCAL Console console = {};

LOCAL glm::vec3 console_input_bg_color = glm::vec3(0.075f, 0.39f, 0.37f);
LOCAL glm::vec3 console_cursor_color = glm::vec3(0.35f, 0.65f, 0.26f);
LOCAL glm::vec3 console_text_color = glm::vec3(0.9f);
LOCAL glm::vec3 console_bg_color = glm::vec3(0.17f, 0.035f, 0.2f);

LOCAL void console_load_command_history(void);
LOCAL bool console_exec_from_argv(u32 argc, char **argv);
LOCAL bool console_exec_from_input(void);
LOCAL void console_handle_backspace(void);
LOCAL bool console_normal_process_key(u16 key);
LOCAL bool console_reverse_search_process_key(u16 key);
LOCAL bool console_process_key(u16 key);
LOCAL void console_reverse_search_update(void);
LOCAL bool console_reverse_search_find_next(void);

extern Log_Registry log_registry;

void console_init(void)
{
    console.input_state = INPUT_NORMAL;
    console.font_size = FA16;
    console.logs = (char **) darray_create(sizeof(char *));

    console_load_command_history();
    cmd_register_all();
}

void console_shutdown(void)
{
    darray_destroy(console.history);
    darray_destroy(console.logs);

    command_manager_cleanup();
}

void console_update(Renderer2D *renderer2d, const glm::mat4 *ui_projection, u32 ww, u32 wh, f32 dt)
{
    if (console.state == CONSOLE_HIDDEN && !console.is_changing_state) {
        return;
    }

    if (console.current_height != console.target_height) {
        PERSIST f32 speed = 15.0;
        PERSIST f32 epsilon = 0.1f;
        f32 dir = console.target_height - console.current_height;
        console.current_height += dt * speed * dir;
        if ((dir > 0.0f && console.current_height >= console.target_height - epsilon) ||
            (dir < 0.0f && console.current_height <= console.target_height + epsilon)) {
            console.current_height = console.target_height;
            console.is_changing_state = false;
        }
    }

    f32 font_width = (f32) renderer2d_get_font_width(renderer2d, console.font_size);
    f32 font_height = (f32) renderer2d_get_font_height(renderer2d, console.font_size);

    glm::vec2 console_size = glm::vec2(
        glm::min(CONSOLE_MAX_WIDTH, (f32) ww - CONSOLE_PAD_X * 2.0f),
        console.current_height
    );

    glm::vec2 console_position = glm::vec2(
        0.0f,
        (f32) wh * 0.5f - console.current_height * 0.5f
    );

    glm::vec2 input_box_size = glm::vec2(
        console_size.x,
        CONSOLE_INPUT_PAD_Y_RATIO_TO_FONT * font_height
    );

    glm::vec2 input_box_position = glm::vec2(
        0.0f,
        glm::round(console_position.y - console_size.y * 0.5f + input_box_size.y * 0.5f)
    );

    glm::vec2 input_text_position = glm::vec2(
        input_box_position.x - input_box_size.x * 0.5f + CONSOLE_INPUT_PAD_X,
        input_box_position.y - ((f32) renderer2d_get_font_bearing_y(renderer2d, console.font_size) * 0.5f)
    );

    input_text_position.y = (input_text_position.y > 0.0f)
                            ? glm::floor(input_text_position.y) + 1.0f
                            : glm::ceil(input_text_position.y);

    renderer2d_begin_scene(renderer2d, ui_projection);

    renderer2d_draw_quad(renderer2d, console_position, console_size, console_bg_color);
    renderer2d_draw_quad(renderer2d, input_box_position, input_box_size, console_input_bg_color);

    glm::vec2 cursor_position;
    glm::vec2 cursor_size;

    if (console.input_state == INPUT_NORMAL) {
        char text_buffer[CONSOLE_PROMPT_LEN + CONSOLE_MAX_INPUT_SIZE + 1] = {};
        snprintf(text_buffer, sizeof(text_buffer), "%s%.*s", CONSOLE_PROMPT, console.cursor, console.input);
        renderer2d_draw_text(renderer2d, text_buffer, console.font_size, input_text_position, console_text_color);

        cursor_position = glm::vec2(
            input_text_position.x + (f32) (CONSOLE_PROMPT_LEN + console.cursor) * font_width + font_width * 0.5f,
            input_box_position.y
        );
        cursor_size = glm::vec2(font_width, font_height);
    } else if (console.input_state == INPUT_REVERSE_SEARCH) {
        PERSIST const char *failed_as_cstr = "failed-";
        PERSIST u32 failed_as_cstr_len = (u32) strlen(failed_as_cstr);
        PERSIST const char *reverse_search_as_cstr = "reverse-search";
        PERSIST u32 reverse_search_as_cstr_len = (u32) strlen(reverse_search_as_cstr);

        char text_buffer[32 + CONSOLE_MAX_INPUT_SIZE + 1] = {};
        snprintf(text_buffer, sizeof(text_buffer), "(%s%s)'%.*s`: %.*s",
                 console.reverse_search_failed ? failed_as_cstr : "",
                 reverse_search_as_cstr,
                 console.cursor, console.input,
                 console.reverse_search_result_length, console.reverse_search_result);
        renderer2d_draw_text(renderer2d, text_buffer, console.font_size, input_text_position, console_text_color);

        if (console.cursor == 0) {
            cursor_position = glm::vec2(
                input_text_position.x + (f32) (console.cursor + reverse_search_as_cstr_len + 6) * font_width + font_width * 0.5f,
                input_box_position.y
            );
            cursor_size = glm::vec2(font_width, font_height);
        } else {
            u32 offset = (failed_as_cstr_len * console.reverse_search_failed) + (reverse_search_as_cstr_len + 6) + console.cursor + console.reverse_search_start_index;
            cursor_size = glm::vec2(
                font_width * (console.reverse_search_failed ? 1.0f : (f32) console.cursor),
                font_height
            );
            cursor_position = glm::vec2(
                input_text_position.x + (f32) offset * font_width + cursor_size.x * 0.5f,
                input_box_position.y
            );
        }
    }

    // Cursor
    console.cursor_blink_accumulator += dt;
    if (console.cursor_blink_accumulator >= CONSOLE_CURSOR_BLINK_PERIOD) {
        console.cursor_visible = !console.cursor_visible;
        console.cursor_blink_accumulator = 0.0f;
    }

    if (console.cursor_visible || (console.input_state == INPUT_REVERSE_SEARCH && console.cursor > 0)) {
        renderer2d_draw_quad(renderer2d, cursor_position, cursor_size, console_cursor_color);
    }

    // Logs
    u64 num_logs = darray_length(console.logs);
    for (i64 i = num_logs - 1, j = 0; i >= 0; i--, j++) {
        char *log = console.logs[i];
        u32 max_chars_per_line = (u32) ((console_size.x - CONSOLE_LOG_PAD_X * 2.0f) / font_width);
        if (max_chars_per_line == 0) {
            break;
        }
        u32 log_num_chars = (u32) strlen(log);
        if (log[log_num_chars-1] == '\n') {
            log_num_chars--;
        }
        if (log_num_chars > max_chars_per_line) {
            // Handle multi-line logs.
            String_View log_sv = sv_from_cstr(log);
            u32 log_total_num_lines = log_num_chars / max_chars_per_line;

            for (i32 k = log_total_num_lines; k >= 0; k--) {
                String_View log_sv_copy = log_sv;
                log_sv_copy.count = log_num_chars >= max_chars_per_line ? max_chars_per_line : log_num_chars;
                log_num_chars -= max_chars_per_line;

                glm::vec2 log_text_position = glm::vec2(
                    input_box_position.x - input_box_size.x * 0.5f + CONSOLE_LOG_PAD_X,
                    glm::round(input_box_position.y + input_box_size.y * 0.5f + ((f32) (j+k) * font_height) + CONSOLE_LOG_PAD_Y)
                );

                if (log_text_position.y >= (f32) wh * 0.5f) {
                    // No more space to render more logs. Continue to the next line.
                    log_sv.data += log_sv_copy.count;
                    continue;
                }

                renderer2d_draw_text(renderer2d, log_sv_copy, console.font_size, log_text_position, console_text_color);
                log_sv.data += log_sv_copy.count;
            }

            j += log_total_num_lines;
        } else {
            // Handle single-line logs.
            glm::vec2 log_text_position = glm::vec2(
                input_box_position.x - input_box_size.x * 0.5f + CONSOLE_LOG_PAD_X,
                glm::round(input_box_position.y + input_box_size.y * 0.5f + ((f32) j * font_height) + CONSOLE_LOG_PAD_Y)
            );

            if (log_text_position.y >= (f32) wh * 0.5f) {
                // No more space to render more logs.
                break;
            }

            renderer2d_draw_text(renderer2d, console.logs[i], console.font_size, log_text_position, console_text_color);
        }
    }

    renderer2d_end_scene(renderer2d);
}

bool console_on_key_pressed_event(Event_Code code, Event_Data data)
{
    UNUSED(code);

    u16 key = data.U16[0];
    if (key == KEYCODE_F1) {
        if (console.state == CONSOLE_HALF_OPEN) {
            console.state = CONSOLE_HIDDEN;
            console.target_height = 0.0f;
            console.is_changing_state = true;
        } else {
            console.state = CONSOLE_HALF_OPEN;
            console.target_height = 300.0f;
            console.is_changing_state = true;
            console.cursor_blink_accumulator = -0.5f; // Make cursor be visible a little longer when opening console
            console.cursor_visible = true;
        }

        return true;
    } else if (key == KEYCODE_F2) {
        if (console.state == CONSOLE_FULL_OPEN) {
            console.state = CONSOLE_HIDDEN;
            console.target_height = 0.0f;
            console.is_changing_state = true;
        } else {
            console.state = CONSOLE_FULL_OPEN;
            console.target_height = 600.0f;
            console.is_changing_state = true;
            console.cursor_blink_accumulator = -0.5f; // Make cursor be visible a little longer when opening console
            console.cursor_visible = true;
        }

        return true;
    } else if (key == KEYCODE_R && input_is_key_pressed(KEYCODE_LeftControl)) {
        if (console.state == CONSOLE_HIDDEN) {
            return false;
        }

        if (console.input_state == INPUT_REVERSE_SEARCH && console.cursor > 0) {
            console.reverse_search_result_index += 1;
            console_reverse_search_find_next();
        } else {
            console.reverse_search_result_index = 0;
            console.input_state = INPUT_REVERSE_SEARCH;
        }
        return true;
    } else if (key == KEYCODE_C && input_is_key_pressed(KEYCODE_LeftControl)) {
        if (console.state == CONSOLE_HIDDEN) {
            return false;
        }

        console.input_state = INPUT_NORMAL;
        console.cursor = 0;
        mem_zero(console.input, sizeof(console.input));
        console.reverse_search_failed = false;
        console.reverse_search_start_index = 0;
        console.reverse_search_result_length = 0;
        mem_zero(console.reverse_search_result, sizeof(console.reverse_search_result));
        return true;
    } else {
        if (console_process_key(key)) {
            return true;
        }
    }

    return false;
}

bool console_on_key_repeated_event(Event_Code code, Event_Data data)
{
    UNUSED(code);

    u16 key = data.U16[0];
    if (console_process_key(key)) {
        return true;
    }

    return false;
}

bool console_on_char_pressed_event(Event_Code code, Event_Data data)
{
    UNUSED(code);

    if (console.state == CONSOLE_HIDDEN) {
        return false;
    }

    if (console.cursor >= CONSOLE_MAX_INPUT_SIZE) {
        return true;
    }

    console.input[console.cursor++] = (char) data.U32[0];
    console.cursor_blink_accumulator = 0.0f;
    console.cursor_visible = true;

    if (console.input_state == INPUT_REVERSE_SEARCH) {
        console_reverse_search_update();
    }

    return true;
}

bool console_on_app_log_event(Event_Code code, Event_Data data)
{
    UNUSED(code); UNUSED(data);
    u64 num_logs = darray_length(log_registry.logs);
    darray_push(console.logs, log_registry.logs[num_logs-1].content);
    return false;
}

LOCAL void console_load_command_history(void)
{
    console.history_cursor = -1;
    console.history = (Console_Cmd *) darray_create(sizeof(Console_Cmd));

    if (!filesystem_open(COMMAND_HISTORY_FILEPATH, FILE_MODE_READ | FILE_MODE_APPEND, false, &console.command_history_file_handle)) {
        LOG_ERROR("failed to open command history file\n");
        return;
    }

    u64 size = 0;
    filesystem_get_size(&console.command_history_file_handle, &size);

    if (size == 0) {
        return;
    }

    u64 start_time = clock_get_absolute_time_ns();

    // TODO: Replace with scratch buffer.
    char *buffer = (char *) mem_alloc(size, MEMORY_TAG_LOG);

    if (!filesystem_read_all(&console.command_history_file_handle, buffer, &size)) {
        LOG_ERROR("failed to read all bytes from command history file\n");
    }

    Console_Cmd cmd = {};

    char *token = strtok(buffer, "\n");
    while (token != NULL) {
        cmd.length = (u32) strlen(token);
        mem_copy(cmd.input, token, cmd.length);
        cmd.input[cmd.length] = '\0';
        darray_push(console.history, cmd);
        token = strtok(NULL, "\n");
    }

    mem_free(buffer, size, MEMORY_TAG_LOG);

    u64 end_time = clock_get_absolute_time_ns();
    LOG_INFO("loaded command history in %f seconds\n", (f32) (end_time - start_time) / 1e9);
}

LOCAL bool console_exec_from_argv(u32 argc, char **argv)
{
    while (argc > 0) {
        const char *flag = shift(&argc, &argv);
        return command_manager_run(flag, argc, argv);
    }

    return true;
}

LOCAL void console_trim_input(void)
{
    i32 start = 0;
    while (console.input[start] != '\0' && isspace(console.input[start])) {
        start++;
    }

    i32 end = console.cursor - 1;
    while (end >= start && isspace(console.input[end])) {
        end--;
    }

    console.cursor = end - start + 1;

    i32 i = 0;
    for (; start <= end; start++) {
        console.input[i++] = console.input[start];
    }
    console.input[i] = '\0';
}

LOCAL bool console_exec_from_input(void)
{
    console_trim_input();
    LOG_INFO("$ %s\n", console.input);

    Console_Cmd cmd = {};
    cmd.length = console.cursor;
    memcpy(cmd.input, console.input, console.cursor);
    darray_push(console.history, cmd);

    char buffer[CONSOLE_MAX_INPUT_SIZE+1] = {};
    mem_copy(buffer, cmd.input, cmd.length);
    buffer[cmd.length] = '\n';
    filesystem_write(&console.command_history_file_handle, buffer, cmd.length+1);

    u32 argc = 0;
    char *argv[CONSOLE_ARGV_CAPACITY] = {};

    char *token = strtok(console.input, " ");
    while (token != NULL) {
        ASSERT_MSG(argc < CONSOLE_ARGV_CAPACITY, "ran out of space for argv");
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }

    bool result = console_exec_from_argv(argc, argv);

    console.cursor = 0;
    memset(console.input, 0, sizeof(console.input));

    return result;
}

LOCAL void console_handle_backspace(void)
{
    if (console.cursor <= 0) {
        return;
    }

    char c;

    if (!input_is_key_pressed(KEYCODE_LeftControl)) {
        console.input[--console.cursor] = '\0';
        goto _end;
    }

    while (console.cursor > 0 && isspace(console.input[console.cursor-1])) {
        console.input[--console.cursor] = '\0';
    }

    if (console.cursor <= 0) {
        return;
    }

    c = console.input[console.cursor-1];
    console.input[--console.cursor] = '\0';
    if (isalnum(c)) {
        while (console.cursor > 0 && isalnum(console.input[console.cursor-1])) {
            console.input[--console.cursor] = '\0';
        }
    }

_end:
    console.cursor_blink_accumulator = 0.0f;
    console.cursor_visible = true;
}

LOCAL bool console_normal_process_key(u16 key)
{
    if (key == KEYCODE_Backspace) {
        console_handle_backspace();
        return true;
    } else if (key == KEYCODE_Enter) {
        console.history_cursor = -1;
        console_exec_from_input();
        return true;
    } else if (key == KEYCODE_Up || (key == KEYCODE_P && input_is_key_pressed(KEYCODE_LeftControl))) {
        u64 len = darray_length(console.history);
        if (console.history_cursor < (i32) len - 1) {
            console.history_cursor += 1;
            Console_Cmd cmd = console.history[len - console.history_cursor - 1];
            memcpy(console.input, cmd.input, cmd.length);
            console.cursor = cmd.length;
            console.input[console.cursor] = '\0';
        }
        return true;
    } else if (key == KEYCODE_Down || (key == KEYCODE_N && input_is_key_pressed(KEYCODE_LeftControl))) {
        u64 len = darray_length(console.history);
        if (console.history_cursor > 0) {
            console.history_cursor -= 1;
            Console_Cmd cmd = console.history[len - console.history_cursor - 1];
            memcpy(console.input, cmd.input, cmd.length);
            console.cursor = cmd.length;
            console.input[console.cursor] = '\0';
        } else if (console.history_cursor == 0) {
            console.history_cursor -= 1;
            memset(console.input, 0, sizeof(console.input));
            console.cursor = 0;
        }
        return true;
    } else if (key == KEYCODE_C && input_is_key_pressed(KEYCODE_LeftControl)) {
        console.history_cursor = -1;
        memset(console.input, 0, sizeof(console.input));
        console.cursor = 0;
    } else if (key >= KEYCODE_Space && key <= KEYCODE_Z) {
        return true;
    }

    return false;
}

LOCAL bool console_reverse_search_process_key(u16 key)
{
    if (key == KEYCODE_Backspace) {
        console_handle_backspace();
        console_reverse_search_update();
        return true;
    } else if (key == KEYCODE_Tab) {
        console.input_state = INPUT_NORMAL;
        console.cursor = console.reverse_search_result_length;
        mem_copy(console.input, console.reverse_search_result, console.reverse_search_result_length);
        mem_zero(console.reverse_search_result, console.reverse_search_result_length);
        console.reverse_search_result_length = 0;
        return true;
    } else if (key >= KEYCODE_Space && key <= KEYCODE_Z) {
        return true;
    }

    return false;
}

LOCAL bool console_process_key(u16 key)
{
    if (console.state == CONSOLE_HIDDEN) {
        return false;
    }

    if (console.input_state == INPUT_NORMAL) {
        return console_normal_process_key(key);
    } else if (console.input_state == INPUT_REVERSE_SEARCH) {
        return console_reverse_search_process_key(key);
    }

    return false;
}

LOCAL void console_reverse_search_update(void)
{
    if (console.cursor == 0) {
        console.reverse_search_result_index = 0;
        console.reverse_search_result_length = 0;
        console.reverse_search_failed = false;
        return;
    }

    if (!console_reverse_search_find_next()) {
        console.reverse_search_start_index = 0;
        console.reverse_search_failed = true;
    }
}

LOCAL bool console_reverse_search_find_next(void)
{
    u32 counter = 0;
    u64 history_len = darray_length(console.history);
    for (i64 i = history_len - 1; i >= 0; i--) {
        Console_Cmd *cmd = &console.history[i];
        char *pos = strcasestr(cmd->input, console.input);
        if (pos != NULL && counter++ >= console.reverse_search_result_index) {
            console.reverse_search_start_index = (u32) (pos - cmd->input);
            console.reverse_search_result_length = cmd->length;
            mem_copy(console.reverse_search_result, cmd->input, cmd->length);
            console.reverse_search_failed = false;
            return true;
        }
    }

    return false;
}
