#ifndef MALLOC_H
#define MALLOC_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void* ac_malloc(size_t size);
extern void ac_free(void* ptr);

extern void MallocInit(void* base, size_t len);
extern void MallocCleanup();
extern int  MallocIsInitalized();
extern void GetFreeArena(u32* max_size, u32* free_size, u32* alloc_size);
extern int CheckArena(); /* @unused */
extern void DisplayArena();

#ifdef __cplusplus
}
#endif

#endif
