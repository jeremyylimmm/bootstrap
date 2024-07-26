#include <stdlib.h>

#include "containers.h"

#define INITIAL_CAPACITY 8

typedef struct {
    size_t capacity;
    size_t length;
} Header;

static Header* get_hdr(void* vec) {
    return (Header*)vec - 1;
}

void vec_destroy(void* vec) {
    if (vec) {
        free(get_hdr(vec));
    }
}

static size_t get_allocation_size(size_t stride, size_t capacity) {
    return sizeof(Header) + stride * capacity;
}

void* vec_push_slot(void* vec, size_t stride) {
    Header* hdr;

    if (vec) {
        hdr = get_hdr(vec);
    }
    else {
        hdr = malloc(get_allocation_size(stride, INITIAL_CAPACITY));
        hdr->capacity = INITIAL_CAPACITY;
        hdr->length = 0;
    }

    if (hdr->length == hdr->capacity) {
        hdr->capacity *= 2;
        hdr = realloc(hdr, get_allocation_size(stride, hdr->capacity));
    }

    hdr->length++;

    return hdr + 1;
}

size_t vec_pop_index(void* vec) {
    assert(vec_len(vec));
    return --get_hdr(vec)->length;
}

size_t vec_len(void* vec) {
    if (vec) {
        return get_hdr(vec)->length;
    }
    else {
        return 0;
    }
}

void vec_clear(void* vec) {
    if (vec) {
        get_hdr(vec)->length = 0;
    }
}