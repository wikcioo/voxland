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
    PACKET_TYPE_PLAYER_BATCH_MOVE,
    NUM_OF_PACKET_TYPES
} Packet_Type;

typedef struct PACKED {
    u32 type;
    u32 payload_size;
} Packet_Header;

typedef struct PACKED {
    u64 time;
} Packet_Ping;

typedef struct PACKED {
    u32 length;
    char *message;
} Packet_Text_Message;

// Required for variable size packets.
u32 serialize_packet_txt_msg(const Packet_Text_Message *packet, u8 **buffer);
void deserialize_packet_txt_msg(void *data, Packet_Text_Message *packet);

typedef struct PACKED {
    u8 username_length;
    char username[PLAYER_USERNAME_MAX_LEN];
    u8 password_length;
    char password[PLAYER_PASSWORD_MAX_LEN];
} Packet_Player_Join_Req;

typedef struct PACKED {
    bool approved;
    player_id id;
    f32 color[3];
    f32 position[3];
} Packet_Player_Join_Res;

typedef struct PACKED {
    player_id id;
    u8 username_length;
    char username[PLAYER_USERNAME_MAX_LEN];
    f32 color[3];
    f32 position[3];
} Packet_Player_Add;

typedef struct PACKED {
    player_id id;
} Packet_Player_Remove;

typedef struct PACKED {
    player_id id;
    f32 position[3];
} Packet_Player_Move;

typedef struct PACKED {
    u32 count;
    player_id *ids;
    f32 *positions; // array of position[3]
} Packet_Player_Batch_Move;

// Required for variable size packets.
u32 serialize_packet_player_batch_move(const Packet_Player_Batch_Move *packet, u8 **buffer);
void deserialize_packet_player_batch_move(void *data, Packet_Player_Batch_Move *packet);

// C++20 does not support array designated initializers...
// so make sure the order is the same as in Packet_Type enum.
LOCAL const u32 PACKET_TYPE_SIZE[NUM_OF_PACKET_TYPES] = {
    0,
    sizeof(Packet_Header),
    sizeof(Packet_Ping),
    sizeof(Packet_Text_Message),
    sizeof(Packet_Player_Join_Req),
    sizeof(Packet_Player_Join_Res),
    sizeof(Packet_Player_Add),
    sizeof(Packet_Player_Remove),
    sizeof(Packet_Player_Move),
    sizeof(Packet_Player_Batch_Move)
};

bool packet_send(i32 socket, u32 type, void *packet_data);
