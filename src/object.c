#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "table.h"
#include "vm.h"

#define ALLOCATE_OBJ(type, objectType) \
    (type*)allocateObject(sizeof(type), objectType)

void printObjectType(ObjType type) {
  switch (type) {
  case OBJ_CLOSURE:      printf("OBJ_CLOSURE");      break;
  case OBJ_FUNCTION:     printf("OBJ_FUNCTION");     break;
  case OBJ_NATIVE:       printf("OBJ_NATIVE");       break;
  case OBJ_STRING:       printf("OBJ_STRING");       break;
  case OBJ_UPVALUE:      printf("OBJ_STRING");       break;
  case OBJ_CLASS:        printf("OBJ_CLASS");        break;
  case OBJ_INSTANCE:     printf("OBJ_INSTANCE");     break;
  case OBJ_BOUND_METHOD: printf("OBJ_BOUND_METHOD"); break;
  }
}

static Obj* allocateObject(size_t size, ObjType type) {
  Obj* object = (Obj*)REALLOCATE(NULL, 0, size);
  object->type = type;
  object->next = vm.objects;
  object->isMarked = false;
  vm.objects = object;
#ifdef DEBUG_LOG_GC
  printf("%p allocate %zu for ", (void*)object, size);
  printObjectType(type);
  printf("\n");
#endif
  return object;
}

ObjBoundMethod* newBoundMethod(Value receiver, ObjClosure* method) {
  ObjBoundMethod* bound = ALLOCATE_OBJ(ObjBoundMethod, OBJ_BOUND_METHOD);
  bound->receiver = receiver;
  bound->method = method;
  return bound;
}

ObjClass* newClass(ObjString* name) {
  ObjClass* klass = ALLOCATE_OBJ(ObjClass, OBJ_CLASS);
  initTable(&klass->methods);
  klass->name = name;
  return klass;
}

ObjClosure* newClosure(ObjFunction* function) {
  ObjUpvalue** upvalues = ALLOCATE(ObjUpvalue*, function->upvalueCount);
  for (int i = 0; i < function->upvalueCount; i++) {
    upvalues[i] = NULL;
  }

  ObjClosure* closure = ALLOCATE_OBJ(ObjClosure, OBJ_CLOSURE);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalueCount = function->upvalueCount;
  return closure;
}

ObjFunction* newFunction() {
  ObjFunction* function = ALLOCATE_OBJ(ObjFunction, OBJ_FUNCTION);
  function->arity = 0;
  function->upvalueCount = 0;
  function->name = NULL;
  initChunk(&function->chunk);
  return function;
}

ObjInstance* newInstance(ObjClass* klass) {
  ObjInstance* instance = ALLOCATE_OBJ(ObjInstance, OBJ_INSTANCE);
  instance->klass = klass;
  initTable(&instance->fields);
  return instance;
}

ObjNative* newNative(NativeFn function, const char* name) {
  ObjNative* native = ALLOCATE_OBJ(ObjNative, OBJ_NATIVE);
  native->function = function;
  native->name = name;
  return native;
}

ObjString* allocateString(int length, uint32_t hash) {
  ObjString* string = (ObjString*)allocateObject(sizeof(ObjString) + sizeof(char) * (length + 1), OBJ_STRING);
  string->length = length;
  string->hash = hash;
  return string;
}

uint32_t hashString(const char* key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

/* ObjString* takeString(char* chars, int length) { */
/*   return allocateString(chars, length); */
/* } */

ObjString* copyString(const char* chars, int length) {
  uint32_t hash = hashString(chars, length);
  ObjString* interned = tableFindString(&vm.strings, chars, length, hash);
  if (interned != NULL) return interned;
  ObjString* string = allocateString(length, hash);
  push(OBJ_VAL(string));
  memcpy(string->chars, chars, length);
  string->chars[length] = '\0';
  tableSet(&vm.strings, OBJ_VAL(string), NIL_VAL);
#ifdef DEBUG_LOG_GC
  printf("-> ");
  printObject(OBJ_VAL(string));
  printf("\n");
#endif
  pop();
  return string;
}

ObjUpvalue* newUpvalue(Value* slot) {
  ObjUpvalue* upvalue = ALLOCATE_OBJ(ObjUpvalue, OBJ_UPVALUE);
  upvalue->closed = NIL_VAL;
  upvalue->location = slot;
  upvalue->next = NULL;
  return upvalue;
}

static void printFunction(ObjFunction* function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }
  printf("<fn %s>", function->name->chars);
}

void printObject(Value value) {
  switch (OBJ_TYPE(value)) {
  case OBJ_BOUND_METHOD:
    printFunction(AS_BOUND_METHOD(value)->method->function);
    break;
  case OBJ_CLASS:
    printf("%s", AS_CLASS(value)->name->chars);
    break;
  case OBJ_CLOSURE:
    printFunction(AS_CLOSURE(value)->function);
    break;
  case OBJ_INSTANCE:
    printf("%s instance", AS_INSTANCE(value)->klass->name->chars);
    break;
  case OBJ_NATIVE:
    printf("<native fn %s>", AS_NATIVE_FUNCTION(value)->name);
    break;
  case OBJ_STRING:
    printf("%s", AS_CSTRING(value));
    break;
  case OBJ_FUNCTION:
    printFunction(AS_FUNCTION(value));
    break;
  case OBJ_UPVALUE:
    printf("upvalue");
    break;
  }
}
