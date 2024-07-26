#include <stdlib.h>
#include <stdio.h>

#include "containers.h"

#define INVALID_INDEX 0xffffffffffffffff

enum {
    EMPTY,
    OCCUPIED,
    REMOVED,
};

static void* get_key(HashSet* set, size_t i) {
    return (uint8_t*)set->keys + i * set->key_size;
}

static void* get_value(void* values, size_t value_size, size_t i) {
    return (uint8_t*)values + i * value_size;
}

HashSet hash_set_new(size_t key_size, ContainerHashFn hash_fn, ContainerCmpFn cmp_fn) {
    return (HashSet) {
        .key_size = key_size,
        .hash_fn = hash_fn,
        .cmp_fn = cmp_fn,
    };
}

void hash_set_destroy(HashSet* set) {
    free(set->states);
    free(set->keys);
}

static float load_factor(size_t used, size_t capacity) {
    return (float)used/(float)capacity;
}

static size_t insert_key(HashSet* set, void* key) {
    uint64_t hash = set->hash_fn(key);
    size_t i = hash % set->capacity;

    for (size_t j = 0; j < set->capacity; ++j) {
        void* cur = get_key(set, i);

        switch (set->states[i]) {
            case EMPTY:
                set->used++;
            case REMOVED:
                set->states[i] = OCCUPIED;
                memcpy(cur, key, set->key_size);
                return i;
            case OCCUPIED:
                if (set->cmp_fn(cur, key)) { return i; }
                break;
        }
        
        i = (i + 1) % set->capacity;
    }

    assert(false);
    return 0xffffffff;
}

static HashSet resized(HashSet* set) {
    HashSet new_set = hash_set_new(set->key_size, set->hash_fn, set->cmp_fn);
    new_set.capacity = set->capacity ? set->capacity * 2 : 8;
    new_set.keys = calloc(new_set.capacity, set->key_size);
    new_set.states = calloc(new_set.capacity, sizeof(uint8_t));
    return new_set;
}

static bool require_resize(HashSet* set) {
    return !set->capacity || load_factor(set->used, set->capacity) > 0.5f;
}

void hash_set_insert(HashSet* set, void* key) {
    if (require_resize(set))
    {
        HashSet new_set = resized(set);

        for (size_t i = 0; i < set->capacity; ++i) {
            if (set->states[i] == OCCUPIED)
            {
                void* cur = get_key(set, i);
                insert_key(&new_set, cur);
            }
        }

        hash_set_destroy(set);
        memcpy(set, &new_set, sizeof(new_set));
    }

    insert_key(set, key);
}

static size_t find(HashSet* set, void* key) {
    if (!set->capacity) {
        return INVALID_INDEX;
    }

    size_t i = set->hash_fn(key) % set->capacity;

    for (size_t j = 0; j < set->capacity; ++j) {
        switch (set->states[i]) {
            case EMPTY:
                return INVALID_INDEX;
            case OCCUPIED: {
                void* cur = get_key(set, i);
                if (set->cmp_fn(cur, key)) {
                    return i;
                }
            } break;
        }
        i = (i+1) % set->capacity;
    }

    return INVALID_INDEX;
}

bool hash_set_contains(HashSet* set, void* key) {
    return find(set, key) != INVALID_INDEX;
}

void hash_set_remove(HashSet* set, void* key) {
    size_t index = find(set, key);

    if (index != INVALID_INDEX) {
        set->states[index] = REMOVED;
    }
}

HashMap hash_map_new(size_t key_size, size_t value_size, ContainerHashFn hash_fn, ContainerCmpFn cmp_fn) {
    return (HashMap) {
        .set = hash_set_new(key_size, hash_fn, cmp_fn),
        .value_size = value_size
    };
}

void hash_map_destroy(HashMap* map) {
    hash_set_destroy(&map->set);
    free(map->values);
}

static void insert_pair(HashSet* set, void* values, size_t value_size, void* key, void* value) {
    size_t idx = insert_key(set, key);
    void* dest = get_value(values, value_size, idx);
    memcpy(dest, value, value_size);
}

void hash_map_insert(HashMap* map, void* key, void* value) {
    if (require_resize(&map->set))
    {
        HashSet new_set = resized(&map->set);
        void* new_values = malloc(map->value_size * new_set.capacity);

        for (size_t i = 0; i < map->set.capacity; ++i) {
            if (map->set.states[i] == OCCUPIED)
            {
                void* cur_key = get_key(&map->set, i);
                void* cur_value = get_value(map->values, map->value_size, i);
                insert_pair(&new_set, new_values, map->value_size, cur_key, cur_value);
            }
        }

        hash_set_destroy(&map->set);
        free(map->values);

        map->set = new_set;
        map->values = new_values;
    }

    insert_pair(&map->set, map->values, map->value_size, key, value);
}

bool hash_map_contains(HashMap* map, void* key) {
    return hash_set_contains(&map->set, key);
}

void hash_map_remove(HashMap* map, void* key) {
    hash_set_remove(&map->set, key);
}

void* hash_map_get(HashMap* map, void* key) {
    size_t idx = find(&map->set, key);
    assert("key does not exist in hash map" && idx != INVALID_INDEX);
    return get_value(map->values, map->value_size, idx);
}