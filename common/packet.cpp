#include "packet.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>

#include "log.h"
#include "asserts.h"

bool packet_send(i32 socket, u32 type, void *packet_data)
{
    ASSERT(type > PACKET_TYPE_NONE && type < NUM_OF_PACKET_TYPES);
    ASSERT(packet_data);

    packet_header_t header = {
        .type = type,
        .payload_size = PACKET_TYPE_SIZE[type]
    };

    // TODO: Replace with arena allocator
    u32 buffer_size = sizeof(packet_header_t) + header.payload_size;
    u8 *buffer = (u8 *) malloc(buffer_size);

    memcpy(buffer, (void *) &header, sizeof(packet_header_t));
    memcpy(buffer + sizeof(packet_header_t), packet_data, header.payload_size);

    // TODO: Change it to add data to the queue instead of sending it straightaway
    i64 bytes_sent_total = 0;
    i64 bytes_sent = 0;
    while (bytes_sent_total < buffer_size) {
        bytes_sent = send(socket, buffer + bytes_sent, buffer_size - bytes_sent, 0);
        if (bytes_sent <= 0) {
            if (bytes_sent == -1) {
                LOG_ERROR("packet_enqueue error: %s", strerror(errno));
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
