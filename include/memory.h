#ifndef MEMORY_H
#define MEMORY_H

#include <stdbool.h>
#include <stdio.h>

#include "common.h"
#include "object.h"


void* reallocate(void* pointer, size_t oldSize, size_t newSize);

typedef struct BlockHeader {
  size_t size;
  bool is_free;
  struct BlockHeader* next;
} BlockHeader;

#define ALIGN_8(size) (((size) + 7) & ~7)

void init_my_heap(size_t total_capacity);
void* my_malloc(size_t size);
void my_free(void* pointer);
void *my_reallocate(void* pointer, size_t oldSize, size_t newSize);
void freeObjects();


#define USE_MY_ALLOCATOR

#ifdef USE_MY_ALLOCATOR
    #define REALLOCATE my_reallocate
#else
    #define REALLOCATE reallocate
#endif

#define ALLOCATE(type, count) \
    (type*)REALLOCATE(NULL, 0, sizeof(type) * (count))

#define FREE(type, pointer) REALLOCATE(pointer, sizeof(type), 0)

#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)REALLOCATE(pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
    REALLOCATE(pointer, sizeof(type) * (oldCount), 0)

#endif
