#pragma once

#include "common/defines.h"

typedef enum {
    MEMORY_TAG_UNKNOWN,
    MEMORY_TAG_STRING,
    MEMORY_TAG_ARRAY,
    MEMORY_TAG_DARRAY,
    MEMORY_TAG_ARENA_ALLOCATOR,
    MEMORY_TAG_RENDERER2D,
    MEMORY_TAG_GAME,
    MEMORY_TAG_OPENGL,
    MEMORY_TAG_UI,
    MEMORY_TAG_NETWORK,
    MEMORY_TAG_CONSOLE,
    MEMORY_TAG_COUNT
} Memory_Tag;

typedef struct {
    u64 total_allocated;
    u64 tagged_allocations[MEMORY_TAG_COUNT];
} Memory_Stats;

void  mem_init(Memory_Stats *ms);
void* mem_alloc(u64 size, Memory_Tag tag);
void  mem_free(void* memory, u64 size, Memory_Tag tag);
void* mem_zero(void* block, u64 size);
void* mem_copy(void* dest, const void* source, u64 size);
void* mem_set(void* dest, i32 value, u64 size);
char* memory_usage_as_cstr(void); // NOTE: Allocates heap memory that should be freed by the user.
