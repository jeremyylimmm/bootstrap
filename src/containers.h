#pragma once

#include "core.h"

#define Vec(T) T*

void vec_destroy(void* vec);
void* vec_push_slot(void* vec, size_t stride);
size_t vec_pop_index(void* vec);
size_t vec_len(void* vec);
void vec_clear(void* vec);

#define vec_push(vec, item) ((*(void**)&vec) = vec_push_slot(vec, sizeof((vec)[0])), (vec)[vec_len(vec)-1] = (item))
#define vec_pop(vec) ((vec)[vec_pop_index(vec)])

typedef uint64_t(*ContainerHashFn)(void*);
typedef bool(*ContainerCmpFn)(void*, void*);

inline uint64_t pointer_hash(void* ptr) {
    return fnv1a(ptr, sizeof(ptr));
}

inline bool pointer_cmp(void* a, void* b) {
    return (*(void**)a) == (*(void**)b);
}

inline uint64_t string_hash(void* ptr) {
    String str = *(String*)ptr;
    return fnv1a(str.str, str.length);
}

inline bool string_cmp(void* a, void* b) {
    String str_a = *(String*)a;
    String str_b = *(String*)b;
    return str_a.length == str_b.length && memcmp(str_a.str, str_b.str, str_a.length) == 0;
}

typedef struct {
    size_t key_size;
    ContainerHashFn hash_fn;
    ContainerCmpFn cmp_fn;

    size_t capacity;
    size_t used;

    void* keys;
    uint8_t* states;
} HashSet;

HashSet hash_set_new(size_t key_size, ContainerHashFn hash_fn, ContainerCmpFn cmp_fn);
void hash_set_destroy(HashSet* set);

void hash_set_insert(HashSet* set, void* key);
bool hash_set_contains(HashSet* set, void* key);
void hash_set_remove(HashSet* set, void* key);

typedef struct {
    HashSet set;

    size_t value_size;
    void* values;
} HashMap;

HashMap hash_map_new(size_t key_size, size_t value_size, ContainerHashFn hash_fn, ContainerCmpFn cmp_fn);
void hash_map_destroy(HashMap* map);

void hash_map_insert(HashMap* map, void* key, void* value);
bool hash_map_contains(HashMap* map, void* key);
void hash_map_remove(HashMap* map, void* key);
void* hash_map_get(HashMap* map, void* key);