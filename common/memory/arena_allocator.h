#pragma once

#include "defines.h"

#ifndef DEFAULT_ARENA_ALIGNMENT
#define DEFAULT_ARENA_ALIGNMENT (1 * sizeof(void *))
#endif

typedef struct {
    void *memory;
    u64 total_size;
    u64 current_offset;
    bool is_memory_owner;
} Arena_Allocator;

void  arena_allocator_create(u64 total_size, void *memory, Arena_Allocator *out_allocator);
void  arena_allocator_destroy(Arena_Allocator *allocator);
void *arena_allocator_allocate(Arena_Allocator *allocator, u64 size);
void *arena_allocator_allocate_align(Arena_Allocator *allocator, u64 size, u64 align);
void  arena_allocator_free_all(Arena_Allocator *allocator);
