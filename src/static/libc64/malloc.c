#include "libc64/__osMalloc.h"
#include "libc64/malloc.h"

OSArena malloc_arena;

extern void* ac_malloc(size_t size) {
    return __osMalloc(&malloc_arena, size);
}

extern void ac_free(void* ptr) {
    __osFree(&malloc_arena, ptr);
}

extern void DisplayArena(void) {
    __osDisplayArena(&malloc_arena);
}

extern void GetFreeArena(u32* max, u32* free, u32* alloc) {
    __osGetFreeArena(&malloc_arena, max, free, alloc);
}
extern void MallocInit(void* start, size_t size) {
    __osMallocInit(&malloc_arena, start, size);
}

extern void MallocCleanup(void) {
    __osMallocCleanup(&malloc_arena);
}
