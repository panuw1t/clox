#ifndef VM_H
#define VM_H

#include "chunk.h"
#include "table.h"
#include "value.h"
#define STACK_MAX 1

typedef struct {
  Chunk* chunk;
  uint8_t* ip;
  Value *stack;
  Value* stackTop;
  Table globals;
  Table constGlobals;
  Table strings;
  Obj* objects;
  int stackSize;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char* source);
void push(Value value);
Value pop();
void negate();

#endif
