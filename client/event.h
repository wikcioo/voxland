#pragma once

#include "common/defines.h"

typedef enum {
    // invalid event code
    EVENT_CODE_NONE,

    // u16 key = data.U16[0];
    EVENT_CODE_KEY_PRESSED,

    // u16 key = data.U16[0];
    EVENT_CODE_KEY_RELEASED,

    // u16 key = data.U16[0];
    EVENT_CODE_KEY_REPEATED,

    // u16 btn = data.U8[0];
    EVENT_CODE_MOUSE_BUTTON_PRESSED,

    // u16 btn = data.U8[0];
    EVENT_CODE_MOUSE_BUTTON_RELEASED,

    // f32 x_pos = data.F32[0];
    // f32 y_pos = data.F32[1];
    EVENT_CODE_MOUSE_MOVED,

    // f32 x_offset = data.F32[0];
    // f32 y_offset = data.F32[1];
    EVENT_CODE_MOUSE_SCROLLED,

    EVENT_CODE_WINDOW_CLOSED,

    // u32 width = data.U32[0];
    // u32 height = data.U32[0];
    EVENT_CODE_WINDOW_RESIZED,

    EVENT_CODE_WINDOW_MINIMIZED,

    EVENT_CODE_WINDOW_MAXIMIZED,

    NUM_OF_EVENT_CODES
} Event_Code;

typedef struct {
    // max 16 bytes of data in a single event
    union {
        u64 U64[2];
        i64 I64[2];
        f64 F64[2];

        u32 U32[4];
        i32 I32[4];
        f32 F32[4];

        u16 U16[8];
        i16 I16[8];

        u8 U8[16];
        i8 I8[16];
    };
} Event_Data;

typedef bool (*pfn_event_callback)(Event_Code code, Event_Data data);

bool event_system_init(void);
void event_system_shutdown(void);

void event_system_register(Event_Code code, pfn_event_callback callback);
void event_system_unregister(Event_Code code, pfn_event_callback callback);
void event_system_fire(Event_Code code, Event_Data data);
