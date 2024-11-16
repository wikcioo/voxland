#include "ring_queue.h"

#include <memory.h>
#include <stdlib.h>

#include "common/asserts.h"
#include "common/memory/memutils.h"

void ring_queue_create(Ring_Queue *ring_queue, u64 stride)
{
    ring_queue_reserve(ring_queue, RING_QUEUE_DEFAULT_CAPACITY, stride);
}

void ring_queue_create_tagged(Ring_Queue *ring_queue, u64 stride, Memory_Tag tag)
{
    ring_queue_reserve_tagged(ring_queue, RING_QUEUE_DEFAULT_CAPACITY, stride, tag);
}

void ring_queue_reserve(Ring_Queue *ring_queue, u64 capacity, u64 stride)
{
    ring_queue_reserve_tagged(ring_queue, capacity, stride, MEMORY_TAG_GENERIC_RING);
}

void ring_queue_reserve_tagged(Ring_Queue *ring_queue, u64 capacity, u64 stride, Memory_Tag tag)
{
    ASSERT(capacity > 0);
    ASSERT(stride > 0);

    ring_queue->capacity = capacity;
    ring_queue->stride = stride;
    ring_queue->head = 0;
    ring_queue->tail = 0;
    ring_queue->count = 0;
    ring_queue->data = mem_alloc(capacity * stride, tag);
    ring_queue->tag = tag;
}

void ring_queue_destroy(Ring_Queue *ring_queue)
{
    ASSERT(ring_queue);

    mem_free(ring_queue->data, ring_queue->capacity * ring_queue->stride, ring_queue->tag);
    mem_zero(ring_queue, sizeof(Ring_Queue));
}

bool ring_queue_enqueue(Ring_Queue *ring_queue, const void *element)
{
    ASSERT(ring_queue);
    ASSERT(element);

    if (ring_queue_is_full(ring_queue)) {
        return false;
    }

    mem_copy((u8 *) ring_queue->data + (ring_queue->head * ring_queue->stride), element, ring_queue->stride);
    ring_queue->head = (ring_queue->head + 1) % ring_queue->capacity;
    ring_queue->count += 1;
    return true;
}

bool ring_queue_dequeue(Ring_Queue *ring_queue, void *out_element)
{
    ASSERT(ring_queue);
    ASSERT(out_element);

    if (ring_queue_is_empty(ring_queue)) {
        return false;
    }

    mem_copy(out_element, (u8 *) ring_queue->data + (ring_queue->tail * ring_queue->stride), ring_queue->stride);
    ring_queue->tail = (ring_queue->tail + 1) % ring_queue->capacity;
    ring_queue->count -= 1;
    return true;
}

bool ring_queue_peek_from_end(Ring_Queue *ring_queue, u64 n, void *out_element)
{
    ASSERT(ring_queue);
    ASSERT(out_element);
    ASSERT(n < ring_queue_length(ring_queue));

    u64 element_index = (ring_queue->tail + n) % ring_queue->capacity;
    mem_copy(out_element, (u8 *) ring_queue->data + (element_index * ring_queue->stride), ring_queue->stride);
    return true;
}

bool ring_queue_is_full(Ring_Queue *ring_queue)
{
    ASSERT(ring_queue);
    return ring_queue->count == ring_queue->capacity;
}

bool ring_queue_is_empty(Ring_Queue *ring_queue)
{
    ASSERT(ring_queue);
    return ring_queue->count == 0;
}

u64 ring_queue_length(Ring_Queue *ring_queue)
{
    ASSERT(ring_queue);
    return ring_queue->count;
}
