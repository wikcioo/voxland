#include "console.h"

#include <stdio.h>
#include <string.h>

#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

#include "client.h"
#include "input_codes.h"
#include "common/log.h"
#include "common/asserts.h"

#define CONSOLE_PROMPT_LEN 2
#define CONSOLE_PROMPT "> "
#define CONSOLE_MAX_INPUT_SIZE 256
#define CONSOLE_ARGV_CAPACITY 32

typedef enum {
    CONSOLE_HIDDEN,
    CONSOLE_HALF_OPEN,
    CONSOLE_FULL_OPEN,
    NUM_OF_CONSOLE_STATES
} Console_State;

typedef struct {
    Console_State state;
    f32 target_height;
    f32 current_height;
    u32 cursor;
    char input[CONSOLE_MAX_INPUT_SIZE+1];
    Font_Atlas_Size font_size;
} Console;

LOCAL Console console = {};

LOCAL char *shift(u32 *argc, char ***argv);
LOCAL bool console_exec_from_argv(u32 argc, char **argv);
LOCAL bool console_exec_from_input(void);

void console_init(void)
{
    console.font_size = FA16;
}

void console_update(Renderer2D *renderer2d, const glm::mat4 *ui_projection, u32 ww, u32 wh, f32 dt)
{
    if (console.current_height != console.target_height) {
        PERSIST f32 speed = 15.0;
        PERSIST f32 epsilon = 0.1f;
        f32 dir = console.target_height - console.current_height;
        console.current_height += dt * speed * dir;
        if ((dir > 0.0f && console.current_height >= console.target_height - epsilon) ||
            (dir < 0.0f && console.current_height <= console.target_height + epsilon)) {
            console.current_height = console.target_height;
        }
    }

    f32 font_height = (f32) renderer2d_get_font_height(renderer2d, console.font_size);

    glm::vec2 console_size = glm::vec2(
        glm::min(1000.0f, (f32) ww - 20.0f),
        console.current_height
    );

    glm::vec2 console_position = glm::vec2(
        0.0f,
        (f32) wh * 0.5f - console.current_height * 0.5f
    );

    glm::vec2 input_box_size = glm::vec2(
        console_size.x,
        font_height + 10.0f
    );

    glm::vec2 input_box_position = glm::vec2(
        0.0f,
        console_position.y - console_size.y * 0.5f + input_box_size.y * 0.5f
    );

    glm::vec2 input_text_position = glm::vec2(
        input_box_position.x - input_box_size.x * 0.5f + 5.0f,
        input_box_position.y - ((f32) renderer2d_get_font_bearing_y(renderer2d, console.font_size) * 0.5f)
    );

    input_text_position.y = (input_text_position.y > 0.0f)
                            ? glm::floor(input_text_position.y) + 1.0f
                            : glm::ceil(input_text_position.y);

    renderer2d_begin_scene(renderer2d, ui_projection);

    renderer2d_draw_quad(renderer2d, console_position, console_size, glm::vec3(0.17f, 0.035f, 0.2f));
    renderer2d_draw_quad(renderer2d, input_box_position, input_box_size, glm::vec3(0.075f, 0.39f, 0.37f));

    char text_buffer[CONSOLE_PROMPT_LEN + CONSOLE_MAX_INPUT_SIZE + 1] = {};
    snprintf(text_buffer, sizeof(text_buffer), "%.*s%.*s", CONSOLE_PROMPT_LEN, CONSOLE_PROMPT, console.cursor, console.input);
    renderer2d_draw_text(renderer2d, text_buffer, console.font_size, input_text_position, glm::vec3(0.9f));

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
        } else {
            console.state = CONSOLE_HALF_OPEN;
            console.target_height = 300.0f;
        }

        return true;
    } else if (key == KEYCODE_F2) {
        if (console.state == CONSOLE_FULL_OPEN) {
            console.state = CONSOLE_HIDDEN;
            console.target_height = 0.0f;
        } else {
            console.state = CONSOLE_FULL_OPEN;
            console.target_height = 600.0f;
        }

        return true;
    } else {
        if (console.state != CONSOLE_HIDDEN) {
            if (key == KEYCODE_Backspace) {
                if (console.cursor > 0) {
                    console.cursor -= 1;
                }
                return true;
            } else if (key >= KEYCODE_Space && key <= KEYCODE_Z) {
                return true;
            } else if (key == KEYCODE_Enter) {
                console_exec_from_input();
            }
        }
    }

    return false;
}

bool console_on_key_repeated_event(Event_Code code, Event_Data data)
{
    UNUSED(code);

    u16 key = data.U16[0];
    if (key == KEYCODE_Backspace && console.state != CONSOLE_HIDDEN) {
        if (console.cursor > 0) {
            console.cursor -= 1;
        }

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

    return true;
}

LOCAL char *shift(u32 *argc, char ***argv)
{
    ASSERT(*argc > 0);
    char *result = **argv;
    *argc -= 1;
    *argv += 1;
    return result;
}

LOCAL bool console_exec_from_argv(u32 argc, char **argv)
{
    while (argc > 0) {
        const char *flag = shift(&argc, &argv);

        if (strcmp(flag, "reload") == 0) {
            if (reload_libgame()) {
                LOG_INFO("successfully reloaded libgame\n");
                return true;
            } else {
                LOG_ERROR("failed to reload libgame\n");
                return false;
            }
        } else {
            LOG_ERROR("unknown flag: `%s`\n", flag);
            return false;
        }
    }

    return true;
}

LOCAL bool console_exec_from_input(void)
{
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
