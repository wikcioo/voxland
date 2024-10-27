#include "packet.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "log.h"
#include "asserts.h"

u32 serialize_packet_txt_msg(const Packet_Text_Message *packet, u8 **buffer)
{
    u32 payload_size = sizeof(packet->length) + packet->length;
    *buffer = (u8 *) malloc(sizeof(Packet_Header) + payload_size);

    memcpy(*buffer + sizeof(Packet_Header), &packet->length, sizeof(packet->length));
    memcpy(*buffer + sizeof(Packet_Header) + sizeof(packet->length), packet->message, packet->length);

    return payload_size;
}

void deserialize_packet_txt_msg(void *data, Packet_Text_Message *packet)
{
    u32 length;
    memcpy(&length, data, sizeof(length));
    packet->length = length;

    // Assuming the lifetime of void *data is longer than packet->message.
    packet->message = (char *) ((u8 *) data + sizeof(length));
}

u32 serialize_packet_player_batch_move(const Packet_Player_Batch_Move *packet, u8 **buffer)
{
    u32 payload_size = (u32) (sizeof(packet->count) + (packet->count * sizeof(player_id)) + (packet->count * 3 * sizeof(f32)));
    *buffer = (u8 *) malloc(sizeof(Packet_Header) + payload_size);

    u32 offset = sizeof(Packet_Header);

    memcpy(*buffer + offset, &packet->count, sizeof(packet->count));
    offset += sizeof(packet->count);

    memcpy(*buffer + offset, packet->ids, packet->count * sizeof(player_id));
    offset += (u32) (packet->count * sizeof(player_id));

    memcpy(*buffer + offset, packet->positions, packet->count * 3 * sizeof(f32));

    return payload_size;
}

void deserialize_packet_player_batch_move(void *data, Packet_Player_Batch_Move *packet)
{
    u32 count;
    memcpy(&count, data, sizeof(count));
    packet->count = count;

    // Assuming the lifetime of void *data is longer than packet->ids.
    packet->ids = (player_id *) ((u8 *) data + sizeof(count));

    // Assuming the lifetime of void *data is longer than packet->positions.
    packet->positions = (f32 *) ((u8 *) data + sizeof(count) + count * sizeof(player_id));
}

bool packet_send(i32 socket, u32 type, void *packet_data)
{
    ASSERT(type > PACKET_TYPE_NONE && type < NUM_OF_PACKET_TYPES);
    ASSERT(packet_data);

    Packet_Header header = { .type = type, .payload_size = 0 };
    u8 *buffer = NULL;

    // Handle variable size packets differently.
    // NOTE: serialize_packet_* functions allocate memory for both header and payload
    switch (type) {
        case PACKET_TYPE_TXT_MSG: {
            header.payload_size = serialize_packet_txt_msg((Packet_Text_Message *) packet_data, &buffer);
        } break;
        case PACKET_TYPE_PLAYER_BATCH_MOVE: {
            header.payload_size = serialize_packet_player_batch_move((Packet_Player_Batch_Move *) packet_data, &buffer);
        } break;
        default: {
            // The packet is fixed size.
            header.payload_size = PACKET_TYPE_SIZE[type];
            buffer = (u8 *) malloc(sizeof(Packet_Header) + header.payload_size);
            memcpy(buffer + sizeof(Packet_Header), packet_data, header.payload_size);
        }
    }

    ASSERT(buffer != NULL);

    memcpy(buffer, (void *) &header, sizeof(Packet_Header));
    u32 buffer_size = sizeof(Packet_Header) + header.payload_size;

    // TODO: Change it to add data to the queue instead of sending it straightaway
    i64 bytes_sent_total = 0;
    i64 bytes_sent = 0;
    while (bytes_sent_total < buffer_size) {
        bytes_sent = send(socket, buffer + bytes_sent, buffer_size - bytes_sent, 0);
        if (bytes_sent <= 0) {
            if (bytes_sent == -1) {
                LOG_ERROR("packet_enqueue error: %s\n", strerror(errno));
            } else {
                LOG_ERROR("server unexpectedly performed orderly shutdown\n");
            }
            return false;
        }
        bytes_sent_total += bytes_sent;
    }

    free(buffer);

    return bytes_sent_total == buffer_size;
}
