#pragma once

#include <stdint.h>
#include <assert.h>
#include <stdbool.h>
#include <memory.h>

#define ARRAY_LENGTH(arr) (sizeof(arr)/sizeof((arr)[0]))

// Defined in os file

typedef struct Arena Arena;
Arena* arena_new();
void arena_destroy(Arena* arena);

void* arena_push(Arena* arena, size_t amount);
void* arena_zero(Arena* arena, size_t amount);

#define arena_type(arena, type) ((type*)arena_zero(arena, sizeof(type)))
#define arena_array(arena, type, count) ((type*)arena_zero(arena, (count) * sizeof(type)))

typedef struct {
    Arena* arena;
    size_t save;
} Scratch;

typedef struct {
    Arena* arenas[2];
} ScratchLibrary;

inline ScratchLibrary scratch_library_new() {
    ScratchLibrary lib = {0};

    for (int i = 0; i < ARRAY_LENGTH(lib.arenas); ++i) {
        lib.arenas[i] = arena_new();
    }

    return lib;
}

inline void scratch_library_destroy(ScratchLibrary* lib) {
    for (int i = 0; i < ARRAY_LENGTH(lib->arenas); ++i) {
        arena_destroy(lib->arenas[i]);
    }
}

Scratch* scratch_get(ScratchLibrary* lib, int num_conflicts, Arena** conflicts);
void scratch_release(Scratch* scratch);

inline uint64_t fnv1a(void* data, size_t n) {
    uint64_t hash = 0xcbf29ce484222325; // Offset basis

    for (size_t i = 0; i < n; ++i) {
        uint8_t b = ((uint8_t*)data)[i];
        hash = (hash & 0xffffffffffffff00) | ((uint8_t)hash ^ b);
        hash *= 0x100000001b3;
    }

    return hash;
}

typedef struct {
    size_t num_bits;
    uint64_t* words;
} Bitset;

inline Bitset* bitset_alloc(Arena* arena, size_t num_bits) {
    Bitset* set = arena_type(arena, Bitset);
    set->num_bits = num_bits;
    size_t num_words = (num_bits + 63) / 64;
    set->words = arena_array(arena, uint64_t, num_words);
    return set;
}

inline bool bitset_get(Bitset* set, size_t index) {
    assert(index < set->num_bits);
    return (set->words[index/64] >> (index % 64)) & 1;
}

inline void bitset_set(Bitset* set, size_t index) {
    assert(index < set->num_bits);
    set->words[index/64] |= ((uint64_t)1<< (index % 64));
}

inline void bitset_unset(Bitset* set, size_t index) {
    assert(index < set->num_bits);
    set->words[index/64] &= ~((uint64_t)1 << (index % 64));
}