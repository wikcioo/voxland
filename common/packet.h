#pragma once

#include "defines.h"

typedef enum {
    PACKET_TYPE_NONE,
    PACKET_TYPE_HEADER,
    PACKET_TYPE_PING,
    PACKET_TYPE_TXT_MSG,
    NUM_OF_PACKET_TYPES
} packet_type_e;

typedef struct PACKED {
    u32 type;
    u32 payload_size;
} packet_header_t;

typedef struct PACKED {
    u64 time;
} packet_ping_t;

typedef struct PACKED {
    u32 length;
    char *message;
} packet_txt_msg_t;

// Required for variable size packets
u32 serialize_packet_txt_msg(const packet_txt_msg_t *packet, u8 **buffer);
void deserialize_packet_txt_msg(void *data, packet_txt_msg_t *packet);

// C++20 does not support array designated initializers...
// so make sure the order is the same as in packet_type_e enum.
LOCAL const u32 PACKET_TYPE_SIZE[NUM_OF_PACKET_TYPES] = {
    0,
    sizeof(packet_header_t),
    sizeof(packet_ping_t),
    sizeof(packet_txt_msg_t)
};

bool packet_send(i32 socket, u32 type, void *packet_data);
