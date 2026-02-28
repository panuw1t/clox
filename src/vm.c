#include <stdio.h>
#include "compiler.h"
#include "common.h"
#include "vm.h"
#include "debug.h"
#include "memory.h"

VM vm;

static void resetStack() {
  vm.stackTop = vm.stack;
}

void initVM() {
  vm.stackSize = STACK_MAX;
  vm.stack = NULL;
  vm.stack = GROW_ARRAY(Value, vm.stack, 0, STACK_MAX);
  resetStack();
}

void freeVM() {
}

void push(Value value) {
  int count = vm.stackTop - vm.stack;
  if (count >= vm.stackSize) {
    int oldCapacity = vm.stackSize;
    vm.stackSize = GROW_CAPACITY(oldCapacity);
    vm.stack = GROW_ARRAY(Value, vm.stack, oldCapacity, vm.stackSize);
    vm.stackTop = vm.stack + count;
  }
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}

void negate() {
  Value* p = vm.stackTop - 1;
  *p = -(*p);
}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_CONSTANT_LONG() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(op) \
    do { \
      double b = pop(); \
      double a = pop(); \
      push(a op b); \
    } while (false)

for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
  printf("          ");
  for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
    printf("[ ");
    printValue(*slot);
    printf(" ]");
  }
  printf("\n");
  disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif // DEBUG_TRACE_EXECUTION

  uint8_t instruction;
  switch (instruction = READ_BYTE()) {
  case OP_CONSTANT: {
    Value constant = READ_CONSTANT();
    push(constant);
    break;
  }
  case OP_CONSTANT_LONG: {
    Value constant = READ_CONSTANT_LONG();
    push(constant);
    break;
  }
  case OP_ADD:      BINARY_OP(+); break;
  case OP_SUBTRACT: BINARY_OP(-); break;
  case OP_MULTIPLY: BINARY_OP(*); break;
  case OP_DIVIDE:   BINARY_OP(/); break;
  case OP_NEGATE:   negate(); break;
  case OP_RETURN: {
    printValue(pop());
    printf("\n");
    return INTERPRET_OK;
  }
  }
}

#undef READ_CONSTANT
#undef READ_BYTE
#undef BINARY_OP
}

InterpretResult interpret(const char* source) {
  compile(source);
  return INTERPRET_OK;
}
