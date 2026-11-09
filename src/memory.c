#include <stdlib.h>

#include "memory.h"
#include "vm.h"
#include "compiler.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#include "object.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  void* result = realloc(pointer, newSize);
  if (result == NULL) exit(1);
  return result;
}

void markObject(Obj* object) {
  if (object == NULL) return;
  if (object->isMarked) return;
#ifdef DEBUG_LOG_GC
  printf("%p mark ", (void*)object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif
  object->isMarked = true;

  if (vm.grayCapacity < vm.grayCount + 1) {
    vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
    vm.grayStack = (Obj**)realloc(vm.grayStack, sizeof(Obj*) * vm.grayCapacity);
    if (vm.grayStack == NULL) exit(1);
  }

  vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value) {
  if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

static void markArray(ValueArray* array) {
  for (int i = 0; i < array->count; i++) {
    markValue(array->values[i]);
  }
}

static void blackenObject(Obj* object) {
#ifdef DEBUG_LOG_GC
  printf("%p blacken ", (void*)object);
  printValue(OBJ_VAL(object));
  printf("\n");
#endif
 switch (object->type) {
 case OBJ_CLASS: {
   ObjClass* klass = (ObjClass*)object;
   markObject((Obj*)klass->name);
   break;
 }
 case OBJ_CLOSURE: {
    ObjClosure* closure = (ObjClosure*)object;
    markObject((Obj*)closure->function);
    for (int i = 0; i < closure->upvalueCount; i++) {
      markObject((Obj*)closure->upvalues[i]);
    }
    break;
  }
  case OBJ_FUNCTION: {
    ObjFunction* function = (ObjFunction*)object;
    markObject((Obj*)function->name);
    markArray(&function->chunk.constants);
    break;
  }
 case OBJ_INSTANCE: {
   ObjInstance* instance = (ObjInstance*)object;
   markObject((Obj*)instance->klass);
   markTable(&instance->fields);
   break;
 }
  case OBJ_UPVALUE:
    markValue(((ObjUpvalue*)object)->closed);
    break;
  case OBJ_NATIVE:
  case OBJ_STRING:
    break;
  }
}

static void freeObject(Obj* object) {
#ifdef DEBUG_LOG_GC
  printf("%p free type ", (void*)object);
  printObjectType(object->type);
  printf("  -> ");
  printObject(OBJ_VAL(object));
  printf("\n");
#endif
  switch (object->type) {
  case OBJ_CLASS: {
    FREE(ObjClass, object);
    break;
  }
  case OBJ_INSTANCE: {
    ObjInstance* instance = (ObjInstance*)object;
    freeTable(&instance->fields);
    FREE(ObjInstance, object);
    break;
  }
  case OBJ_NATIVE:
    FREE(ObjNative, object);
    break;
  case OBJ_STRING: {
    ObjString* string = (ObjString*)object;
    FREE(ObjString, object);
    break;
  }
  case OBJ_CLOSURE: {
    ObjClosure* closure = (ObjClosure*)object;
    FREE_ARRAY(ObjUpvalue*, closure->upvalues, closure->upvalueCount);
    FREE(ObjClosure, object);
    break;
  }
  case OBJ_FUNCTION: {
    ObjFunction* function = (ObjFunction*)object;
    freeChunk(&function->chunk);
    FREE(ObjFunction, object);
    break;
  }
  case OBJ_UPVALUE:
    FREE(ObjUpvalue, object);
    break;
  }
}

static void markRoots() {
  for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
    markValue(*slot);
  }

  for (int i = 0; i < vm.frameCount; i++) {
    markObject((Obj*)vm.frames[i].closure);
  }

  for (ObjUpvalue* upvalue = vm.openUpvalues;
       upvalue != NULL;
       upvalue = upvalue->next) {
    markObject((Obj*)upvalue);
  }

  markArray(&vm.globals);

  markTable(&vm.globalIndices);

  markCompilerRoots();
}

static void traceReferences() {
  while (vm.grayCount > 0) {
    Obj* object = vm.grayStack[--vm.grayCount];
    blackenObject(object);
  }
}

static void sweep() {
  Obj* previous = NULL;
  Obj* object = vm.objects;
  while (object != NULL) {
    if (object->isMarked) {
      object->isMarked = false;
      previous = object;
      object = object->next;
    } else {
      Obj* unreached = object;
      object = object->next;
      if (previous != NULL) {
        previous->next = object;
      } else {
        vm.objects = object;
      }

      freeObject(unreached);
    }
  }
}

void collectGarbage() {
#ifdef DEBUG_LOG_GC
  printf("-- gc begin\n");
  size_t before = vm.bytesAllocated;
#endif
  markRoots();
  traceReferences();
  tableRemoveWhite(&vm.strings);
  sweep();

  vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
  printf("-- gc end\n");
  printf("   collected %zu bytes (from %zu to %zu) next at %zu\n",
         before - vm.bytesAllocated, before, vm.bytesAllocated,
         vm.nextGC);
#endif
}

void freeObjects() {
  Obj* object = vm.objects;
  while (object != NULL) {
    Obj* next = object->next;
    freeObject(object);
    object = next;
  }

  free(vm.grayStack);
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
  if (pointer == NULL) return;
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
  vm.bytesAllocated += newSize - oldSize;
  if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
    collectGarbage();
#endif
  }
  if (vm.bytesAllocated > vm.nextGC) {
    collectGarbage();
  }
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
