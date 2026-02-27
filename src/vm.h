#ifndef VM_H
#define VM_H

#include "chunk.h"
#define STACK_MAX 1

typedef struct {
  Chunk* chunk;
  uint8_t* ip;
  Value *stack;
  Value* stackTop;
  int stackSize;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initVM();
void freeVM();
InterpretResult interpret(Chunk* chunk);
void push(Value value);
Value pop();
void negate();

#endif
