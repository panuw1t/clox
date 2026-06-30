#ifndef MEMORY_H
#define MEMORY_H

#include "common.h"
#include <stdbool.h>
#include <stdio.h>
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


#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))

#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)

void* reallocate(void* pointer, size_t oldSize, size_t newSize);

#endif
