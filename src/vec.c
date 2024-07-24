#include <stdlib.h>

#include "containers.h"

Vec vec_new(size_t stride) {
    return (Vec) {
        .stride = stride,
    };
}

void vec_destroy(Vec* vec) {
    free(vec->memory);
    memset(vec, 0, sizeof(*vec));
}

void* vec_push(Vec* vec, void* data) {
    if (vec->length == vec->capacity) {
        vec->capacity = vec->capacity ? vec->capacity * 2 : 8;
        vec->memory = realloc(vec->memory, vec->capacity * vec->stride);
    }

    size_t index = vec->length++;
    void* dest = (uint8_t*)vec->memory + index * vec->stride;
    memcpy(dest, data, vec->stride);

    return dest;
}

void* vec_pop(Vec* vec) {
    assert(vec->length);
    size_t index = --vec->length;
    return (uint8_t*)vec->memory + index * vec->stride;
}