#include "packet.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "log.h"
#include "asserts.h"

u32 serialize_packet_txt_msg(const packet_txt_msg_t *packet, u8 **buffer)
{
    u32 payload_size = sizeof(packet->length) + packet->length;
    *buffer = (u8 *) malloc(sizeof(packet_header_t) + payload_size);

    memcpy(*buffer + sizeof(packet_header_t), &packet->length, sizeof(packet->length));
    memcpy(*buffer + sizeof(packet_header_t) + sizeof(packet->length), packet->message, packet->length);

    return payload_size;
}

void deserialize_packet_txt_msg(void *data, packet_txt_msg_t *packet)
{
    u32 length;
    memcpy(&length, data, sizeof(length));
    packet->length = length;

    // Assuming the lifetime of void *data is longer than packet->message.
    packet->message = (char *) ((u8 *) data + sizeof(length));
}

bool packet_send(i32 socket, u32 type, void *packet_data)
{
    ASSERT(type > PACKET_TYPE_NONE && type < NUM_OF_PACKET_TYPES);
    ASSERT(packet_data);

    packet_header_t header = { .type = type, .payload_size = 0 };
    u8 *buffer = NULL;

    // Handle variable size packets differently.
    // NOTE: serialize_packet_* functions allocate memory for both header and payload
    switch (type) {
        case PACKET_TYPE_TXT_MSG: {
            header.payload_size = serialize_packet_txt_msg((packet_txt_msg_t *) packet_data, &buffer);
        } break;
        default: {
            // The packet is fixed size.
            header.payload_size = PACKET_TYPE_SIZE[type];
            buffer = (u8 *) malloc(sizeof(packet_header_t) + header.payload_size);
            memcpy(buffer + sizeof(packet_header_t), packet_data, header.payload_size);
        }
    }

    ASSERT(buffer != NULL);

    memcpy(buffer, (void *) &header, sizeof(packet_header_t));
    u32 buffer_size = sizeof(packet_header_t) + header.payload_size;

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
