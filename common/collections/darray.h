#pragma once

#include "defines.h"

/********************************************************************************
 *  Memory layout of darray:                                                    *
 *    u64 capacity - max amount of elements that can be stored without resizing *
 *    u64 length   - current amount of elements in the array                    *
 *    u64 stride   - size of each element in bytes                              *
 *    void *data   - start of actual data                                       *
 ********************************************************************************/

#define DARRAY_DEFAULT_CAPACITY 8
#define DARRAY_DEFAULT_RESIZE_FACTOR 2

enum darray_field {
    DARRAY_FIELD_CAPACITY,
    DARRAY_FIELD_LENGTH,
    DARRAY_FIELD_STRIDE,
    DARRAY_FIELD_COUNT
};

void *_darray_create(u64 capacity, u64 stride);
void  _darray_destroy(void *array);

void *_darray_resize(void *array, u64 new_capacity);

void *_darray_push(void *array, const void *element);
void  _darray_pop(void *array, void *out_element);

void *_darray_push_at(void *array, u64 index, const void *element);
void  _darray_pop_at(void *array, u64 index, void *out_element);

u64  _darray_field_get(void *array, u64 field);
void _darray_field_set(void *array, u64 field, u64 value);

#define darray_create(stride) \
    _darray_create(DARRAY_DEFAULT_CAPACITY, stride)

#define darray_reserve(capacity, stride) \
    _darray_create(capacity, stride)

#define darray_destroy(array) \
    _darray_destroy(array)

// these c++ casts are ugly asf, but needed for warning-less compilation
#define darray_push(array, element)                                                                              \
    {                                                                                                            \
        auto tmp = element;                                                                                      \
        array = static_cast<decltype(array)>(_darray_push(static_cast<void*>(array), static_cast<void*>(&tmp))); \
    }

#define darray_pop(array, out_element) \
    _darray_pop(array, out_element)

#define darray_push_at(array, index, element)                                                                              \
    {                                                                                                                      \
        typeof(element) tmp = element;                                                                                     \
        array = static_cast<decltype(array)>(_darray_push_at(static_cast<void*>(array), index, static_cast<void*>(&tmp))); \
    }

#define darray_pop_at(array, index, out_element) \
    _darray_pop_at(array, index, out_element)

#define darray_clear(array) \
    _darray_field_set(array, DARRAY_FIELD_LENGTH, 0)

#define darray_capacity(array) \
    _darray_field_get(array, DARRAY_FIELD_CAPACITY)

#define darray_length(array) \
    _darray_field_get(array, DARRAY_FIELD_LENGTH)

#define darray_stride(array) \
    _darray_field_get(array, DARRAY_FIELD_STRIDE)
