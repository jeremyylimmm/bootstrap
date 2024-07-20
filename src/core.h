#pragma once

#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <memory.h>

// Defined in os file

typedef struct Arena Arena;
Arena* arena_new();

void* arena_push(Arena* arena, size_t amount);
void* arena_zero(Arena* arena, size_t amount);

#define arena_type(arena, type) ((type*)arena_zero(arena, sizeof(type)))
#define arena_array(arena, type, count) ((type*)arena_zero(arena, (count) * sizeof(type)))