#pragma once

#include "core.h"

typedef struct {
    size_t stride;
    size_t capacity;
    size_t length;
    void* memory;
} Vec;

Vec vec_new(size_t stride);
void vec_destroy(Vec* vec);
void* vec_push(Vec* vec, void* data);
void* vec_pop(Vec* vec);

typedef uint64_t(*ContainerHashFn)(void*);
typedef bool(*ContainerCmpFn)(void*, void*);

typedef struct {
    size_t stride;
    ContainerHashFn hash_fn;
    ContainerCmpFn cmp_fn;

    size_t capacity;
    size_t used;

    void* table;
    uint8_t* states;
} HashSet;

HashSet hash_set_new(size_t stride, ContainerHashFn hash_fn, ContainerCmpFn cmp_fn);
void hash_set_destroy(HashSet* set);

void hash_set_insert(HashSet* set, void* data);
bool hash_set_contains(HashSet* set, void* data);
void hash_set_remove(HashSet* set, void* data);

inline uint64_t pointer_hash(void* ptr) {
    return fnv1a(ptr, sizeof(ptr));
}

inline bool pointer_cmp(void* a, void* b) {
    return (*(void**)a) == (*(void**)b);
}