#include <stdlib.h>

#include "memory.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  void* result = realloc(pointer, newSize);
  if (result == NULL) exit(1);
  return result;
}

static void* heap_start = NULL;
static BlockHeader* free_list_head = NULL;

void init_my_heap(size_t total_capacity) {
  heap_start = malloc(total_capacity);
  if (heap_start == NULL) {
    fprintf(stderr, "Could not allocate heap. %zu\n", total_capacity);
    exit(1);
  }
  BlockHeader* first_block = (BlockHeader*)heap_start;
  first_block->size = total_capacity - sizeof(BlockHeader);
  first_block->is_free = true;
  first_block->next = NULL;
  free_list_head = first_block;
}

void* my_malloc(size_t size) {
  size_t aligned_size = ALIGN_8(size);
  for (BlockHeader* current = free_list_head;current != NULL;current = current->next) {
    if (current->size >= aligned_size && current->is_free) {
      current->is_free = false;
      if (current->size >= aligned_size + sizeof(BlockHeader) + 8) {
        char* header = (char*)current;
        header += sizeof(BlockHeader) + aligned_size;
        BlockHeader* newBlock = (BlockHeader*)header;
        newBlock->is_free = true;
        newBlock->size = current->size - aligned_size - sizeof(BlockHeader);
        newBlock->next = current->next;
        current->next = newBlock;
        current->size = aligned_size;
      }
      return current + 1;
    }
  }
  return NULL;
}

void my_free(void* pointer) {
  BlockHeader* header = (BlockHeader*)((char*)pointer - sizeof(BlockHeader));
  BlockHeader* current = free_list_head;
  BlockHeader* previous = NULL;
  while (current != NULL) {
    if (current == header)
      break;
    previous = current;
    current = current->next;
  }
  if (previous != NULL && previous->is_free) {
    previous->next = header->next;
    previous->size = previous->size + sizeof(BlockHeader) + header->size;
    header = previous;
  }
  if (header->next != NULL && header->next->is_free) {
    header->size = header->size + header->next->size + sizeof(BlockHeader);
    header->next = header->next->next;
  }
  header->is_free = true;
}

void *my_reallocate(void* pointer, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
    my_free(pointer);
    return NULL;
  }

  if (pointer == NULL) {
    return my_malloc(newSize);
  }
  char* newData = (char*)my_malloc(newSize);

  if (newData == NULL) {
    return NULL;
  }

  char* oldData = (char*)pointer;
  size_t minSize = (oldSize < newSize) ? oldSize : newSize;
  for (int i=0;i < minSize;i++) {
    newData[i] = oldData[i];
  }
  my_free(pointer);
  return (void*)newData;
}
