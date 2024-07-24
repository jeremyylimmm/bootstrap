#pragma once

#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <memory.h>

// Defined in os file

typedef struct Arena Arena;
Arena* arena_new();
void arena_destroy(Arena* arena);

void* arena_push(Arena* arena, size_t amount);
void* arena_zero(Arena* arena, size_t amount);

#define arena_type(arena, type) ((type*)arena_zero(arena, sizeof(type)))
#define arena_array(arena, type, count) ((type*)arena_zero(arena, (count) * sizeof(type)))

inline uint64_t fnv1a(void* data, size_t n) {
    uint64_t hash = 0xcbf29ce484222325; // Offset basis

    for (size_t i = 0; i < n; ++i) {
        uint8_t b = ((uint8_t*)data)[i];
        hash = (hash & 0xffffffffffffff00) | ((uint8_t)hash ^ b);
        hash *= 0x100000001b3;
    }

    return hash;
}