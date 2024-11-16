#pragma once

#include "common/defines.h"
#include "common/memory/memutils.h"

#define RING_QUEUE_DEFAULT_CAPACITY 8

typedef struct {
    u64 capacity;
    u64 stride;
    u64 head;
    u64 tail;
    u64 count;
    void *data;
    Memory_Tag tag;
} Ring_Queue;

void ring_queue_create(Ring_Queue *ring_queue, u64 stride);
void ring_queue_create_tagged(Ring_Queue *ring_queue, u64 stride, Memory_Tag tag);
void ring_queue_reserve(Ring_Queue *ring_queue, u64 capacity, u64 stride);
void ring_queue_reserve_tagged(Ring_Queue *ring_queue, u64 capacity, u64 stride, Memory_Tag tag);
void ring_queue_destroy(Ring_Queue *ring_queue);

bool ring_queue_enqueue(Ring_Queue *ring_queue, const void *element);
bool ring_queue_dequeue(Ring_Queue *ring_queue, void *out_element);

bool ring_queue_peek_from_end(void *ring_queue, u64 n, void *out_element);

bool ring_queue_is_full(Ring_Queue *ring_queue);
bool ring_queue_is_empty(Ring_Queue *ring_queue);
u64  ring_queue_length(Ring_Queue *ring_queue);
