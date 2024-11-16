#include "memutils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/log.h"
#include "common/asserts.h"
#include "common/size_unit.h"

LOCAL const char *memory_tag_strings[MEMORY_TAG_COUNT] = {
    "unknown   ",
    "string    ",
    "array     ",
    "darray    ",
    "gen_arena ",
    "gen_ring  ",
    "renderer2d",
    "game      ",
    "opengl    ",
    "ui        ",
    "network   ",
    "console   ",
    "log       "
};

LOCAL Memory_Stats *stats;

void mem_init(Memory_Stats *ms)
{
    stats = ms;
}

void* mem_alloc(u64 size, Memory_Tag tag)
{
    ASSERT_MSG(stats, "mem_alloc: stats not initialized");
    ASSERT(size > 0);
    ASSERT(tag >= MEMORY_TAG_UNKNOWN && tag < MEMORY_TAG_COUNT);

    stats->total_allocated += size;

    if (tag == MEMORY_TAG_UNKNOWN) {
        LOG_WARN("memory allocation of tag 'unknown' - consider re-tagging");
    }

    stats->tagged_allocations[tag] += size;

    void *memory = malloc(size);
    mem_zero(memory, size);
    return memory;
}

void mem_free(void* memory, u64 size, Memory_Tag tag)
{
    ASSERT_MSG(stats, "mem_free: stats not initialized");
    ASSERT(memory);
    ASSERT(size > 0);
    ASSERT(tag >= MEMORY_TAG_UNKNOWN && tag < MEMORY_TAG_COUNT);
    ASSERT(stats->tagged_allocations[tag] > 0);

    stats->tagged_allocations[tag] -= size;
    free(memory);
}

void* mem_zero(void* block, u64 size)
{
    ASSERT(block);
    return memset(block, 0, size);
}

void* mem_copy(void* dest, const void* source, u64 size)
{
    ASSERT(dest);
    ASSERT(source);
    return memcpy(dest, source, size);
}

void* mem_set(void* dest, i32 value, u64 size)
{
    ASSERT(dest);
    return memset(dest, value, size);
}

char* memory_usage_as_cstr(void)
{
    ASSERT_MSG(stats, "memory_usage_as_cstr: stats not initialized");

    char buffer[1024] = "memory usage\n";
    u64 offset = strlen(buffer);
    for (i32 i = 0; i < MEMORY_TAG_COUNT; i++) {
        f32 usage;
        const char *unit = get_size_unit(stats->tagged_allocations[i], &usage);
        i32 length = snprintf(buffer + offset, 1024 - offset, "  %s: %.02f %s%c",
                              memory_tag_strings[i], usage, unit, i < MEMORY_TAG_COUNT-1 ? '\n' : ' ');
        offset += length;
    }

    // TODO: Replace with arena allocator.
    return strdup(buffer);
}
