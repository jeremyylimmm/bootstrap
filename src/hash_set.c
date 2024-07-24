#include <stdlib.h>

#include "containers.h"

enum {
    EMPTY,
    OCCUPIED,
    REMOVED,
};

static void* get_loc(HashSet* set, size_t i) {
    return (uint8_t*)set->table + i * set->stride;
}

HashSet hash_set_new(size_t stride, ContainerHashFn hash_fn, ContainerCmpFn cmp_fn) {
    return (HashSet) {
        .stride = stride,
        .hash_fn = hash_fn,
        .cmp_fn = cmp_fn,
    };
}

void hash_set_destroy(HashSet* set) {
    free(set->states);
    free(set->table);
}

static float load_factor(size_t length, size_t capacity) {
    return (float)length/(float)capacity;
}

static void static_insert(HashSet* set, void* data) {
    uint64_t hash = set->hash_fn(data);
    size_t i = hash % set->capacity;

    for (size_t j = 0; j < set->capacity; ++j) {
        void* cur = get_loc(set, i);

        switch (set->states[i]) {
            case EMPTY:
                set->used++;
            case REMOVED:
                set->states[i] = OCCUPIED;
                memcpy(cur, data, set->stride);
                return;
            case OCCUPIED:
                if (set->cmp_fn(cur, data)) { return; }
                break;
        }
        
        i = (i + 1) % set->capacity;
    }

    assert(false);
}

void hash_set_insert(HashSet* set, void* data) {
    if (!set->capacity || load_factor(set->used, set->capacity) > 0.5f)
    {
        HashSet new_set = hash_set_new(set->stride, set->hash_fn, set->cmp_fn);
        new_set.capacity = set->capacity ? set->capacity * 2 : 8;
        new_set.table = calloc(new_set.capacity, set->stride);
        new_set.states = calloc(new_set.capacity, sizeof(uint8_t));

        for (size_t i = 0; i < set->capacity; ++i) {
            if (set->states[i] == OCCUPIED)
            {
                void* cur = get_loc(set, i);
                static_insert(&new_set, cur);
            }
        }

        hash_set_destroy(set);
        memcpy(set, &new_set, sizeof(new_set));
    }

    static_insert(set, data);
}

bool hash_set_contains(HashSet* set, void* data) {
    if (!set->capacity) {
        return false;
    }

    size_t i = set->hash_fn(data) % set->capacity;

    for (size_t j = 0; j < set->capacity; ++j) {
        switch (set->states[i]) {
            case EMPTY:
                return false;
            case OCCUPIED: {
                void* cur = get_loc(set, i);
                if (set->cmp_fn(cur, data)) {
                    return true;
                }
            } break;
        }
        i = (i+1) % set->capacity;
    }

    return false;
}

void hash_set_remove(HashSet* set, void* data) {
    if (!set->capacity) {
        return;
    }

    size_t i = set->hash_fn(data) % set->capacity;

    for (size_t j = 0; j < set->capacity; ++j) {
        switch (set->states[i]) {
            case EMPTY:
                return;
            case OCCUPIED: {
                void* cur = get_loc(set, i);
                if (set->cmp_fn(cur, data)) {
                    set->states[i] = REMOVED;
                    return;
                }
            } break;
        }
        i = (i+1) % set->capacity;
    }
}