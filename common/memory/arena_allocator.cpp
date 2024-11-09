#include "arena_allocator.h"

#include <stdlib.h>
#include <memory.h>

#include "common/log.h"
#include "common/asserts.h"
#include "common/size_unit.h"

void arena_allocator_create(u64 total_size, void *memory, Arena_Allocator *out_allocator)
{
    arena_allocator_create_tagged(total_size, memory, out_allocator, MEMORY_TAG_GENERIC_ARENA);
}

void arena_allocator_create_tagged(u64 total_size, void *memory, Arena_Allocator *out_allocator, Memory_Tag tag)
{
    ASSERT(out_allocator);

    out_allocator->total_size = total_size;
    out_allocator->current_offset = 0;
    out_allocator->tag = tag;

    if (memory == 0) {
        out_allocator->memory = mem_alloc(total_size, tag);
        out_allocator->is_memory_owner = true;
    } else {
        out_allocator->memory = memory;
        out_allocator->is_memory_owner = false;
    }
}

void arena_allocator_destroy(Arena_Allocator *allocator)
{
    ASSERT(allocator && allocator->memory);

    if (allocator->is_memory_owner) {
        mem_free(allocator->memory, allocator->total_size, allocator->tag);
    }
}

#define IS_POWER_OF_TWO(n) ((n & (n-1)) == 0)

LOCAL u64 align_forward(u64 ptr, u64 align)
{
    ASSERT_MSG(IS_POWER_OF_TWO(align), "align value must be a power of 2");

    u64 modulo = ptr & (align-1);
    if (modulo != 0) {
        // If ptr is not aligned, move the address to the next value which is aligned
        ptr += align - modulo;
    }

    return ptr;
}

void *arena_allocator_allocate(Arena_Allocator *allocator, u64 size)
{
    return arena_allocator_allocate_align(allocator, size, DEFAULT_ARENA_ALIGNMENT);
}

void *arena_allocator_allocate_align(Arena_Allocator *allocator, u64 size, u64 align)
{
    ASSERT(allocator && allocator->memory);

    u64 curr_ptr = (u64) allocator->memory + allocator->current_offset;
    u64 offset = align_forward(curr_ptr, align);
    offset -= (u64) allocator->memory;

    if (offset + size > allocator->total_size) {
        u64 used = offset;
        f32 used_formatted;

        u64 available = allocator->total_size;
        f32 available_formatted;

        const char *used_unit = get_size_unit(used, &used_formatted); UNUSED(used_unit);
        const char *available_unit = get_size_unit(available, &available_formatted); UNUSED(available_unit);
        LOG_ERROR("ran out of memory in arena allocator when trying to allocate %llu bytes: used=%.02f%s / available=%.02f%s\n",
                  size, used_formatted, used_unit, available_formatted, available_unit);
        return NULL;
    }

    void *ptr = (u8 *) allocator->memory + allocator->current_offset;
    allocator->current_offset += size;
    memset(ptr, 0, size);
    return ptr;
}

bool arena_allocator_can_allocate(Arena_Allocator *allocator, u64 size)
{
    return allocator->current_offset + size <= allocator->total_size;
}

void arena_allocator_free_all(Arena_Allocator *allocator)
{
    ASSERT(allocator && allocator->memory);
    allocator->current_offset = 0;
}
