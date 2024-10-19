#pragma once

#include "defines.h"
#include "player_types.h"

typedef enum {
    PACKET_TYPE_NONE,
    PACKET_TYPE_HEADER,
    PACKET_TYPE_PING,
    PACKET_TYPE_TXT_MSG,
    PACKET_TYPE_PLAYER_JOIN_REQ,
    PACKET_TYPE_PLAYER_JOIN_RES,
    PACKET_TYPE_PLAYER_ADD,
    PACKET_TYPE_PLAYER_REMOVE,
    PACKET_TYPE_PLAYER_MOVE,
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

// Required for variable size packets.
u32 serialize_packet_txt_msg(const packet_txt_msg_t *packet, u8 **buffer);
void deserialize_packet_txt_msg(void *data, packet_txt_msg_t *packet);

typedef struct PACKED {
    u8 username_length;
    char username[PLAYER_USERNAME_MAX_LEN];
    u8 password_length;
    char password[PLAYER_PASSWORD_MAX_LEN];
} packet_player_join_req_t;

typedef struct PACKED {
    bool approved;
    player_id id;
    f32 color[3];
    f32 position[3];
} packet_player_join_res_t;

typedef struct PACKED {
    player_id id;
    u8 username_length;
    char username[PLAYER_USERNAME_MAX_LEN];
    f32 color[3];
    f32 position[3];
} packet_player_add_t;

typedef struct PACKED {
    player_id id;
} packet_player_remove_t;

typedef struct PACKED {
    player_id id;
    f32 position[3];
} packet_player_move_t;

// C++20 does not support array designated initializers...
// so make sure the order is the same as in packet_type_e enum.
LOCAL const u32 PACKET_TYPE_SIZE[NUM_OF_PACKET_TYPES] = {
    0,
    sizeof(packet_header_t),
    sizeof(packet_ping_t),
    sizeof(packet_txt_msg_t),
    sizeof(packet_player_join_req_t),
    sizeof(packet_player_join_res_t),
    sizeof(packet_player_add_t),
    sizeof(packet_player_remove_t),
    sizeof(packet_player_move_t)
};

bool packet_send(i32 socket, u32 type, void *packet_data);
