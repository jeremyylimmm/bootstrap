#pragma once

#include <Windows.h>
#include "core.h"
#include <stdio.h>

#define PAGE_LIMIT (1024 * 1024 * 10)

struct Arena {
    void* next_page;
    size_t page_size;

    size_t base;
    size_t used;
    size_t capacity;
};

Arena* arena_new() {
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);

    size_t page_size = system_info.dwPageSize;
    void* memory = VirtualAlloc(0, PAGE_LIMIT * page_size, MEM_RESERVE, PAGE_NOACCESS);
    assert("virtual alloc failed" && memory);

    Arena* arena = LocalAlloc(LMEM_ZEROINIT, sizeof(Arena));
    arena->base = (size_t)memory;
    arena->next_page = memory;
    arena->capacity = 0;
    arena->page_size = page_size;

    return arena;
}

void arena_destroy(Arena* arena) {
    VirtualFree((void*)arena->base, 0, MEM_RELEASE);
    LocalFree(arena);
}

void* arena_push(Arena* arena, size_t amount) {
    if (!amount) {
        return 0;
    }

    amount = (amount + 7) & ~7;

    while (arena->used + amount > arena->capacity) {
        void* result = VirtualAlloc(arena->next_page, arena->page_size, MEM_COMMIT, PAGE_READWRITE);
        assert("memory commit failed" && result);
        arena->next_page = (void*)((size_t)arena->next_page + arena->page_size);
        arena->capacity += arena->page_size;
    }

    size_t result = arena->base + arena->used;
    arena->used += amount;

    return (void*)result;
}

void* arena_zero(Arena* arena, size_t amount) {
    void* result = arena_push(arena, amount);
    memset(result, 0, amount);
    return result;
}