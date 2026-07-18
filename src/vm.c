#include "common.h"
#include "debug.h"
#include "vm.h"
#include "compiler.h"
#include "memory.h"

VM vm;

static void resetStack() {
  vm.stackTop = vm.stack;
}

void initStack() {
  vm.stackCapacity = 0;
  vm.stack = NULL;
  vm.stackTop = NULL;
}

void initVM() {
  initStack();
}

void freeVM() {
}

void push(Value value) {
  if (vm.stackCapacity == 0 || vm.stackCapacity < vm.stackTop - vm.stack + 1) {
    int index = 0;
    if (vm.stack != NULL) index = vm.stackTop - vm.stack;
    int oldCapacity = vm.stackCapacity;
    vm.stackCapacity = GROW_CAPACITY(oldCapacity > 0 ? oldCapacity : STACK_MAX / 2);
    vm.stack = GROW_ARRAY(Value, vm.stack, oldCapacity, vm.stackCapacity);
    vm.stackTop = vm.stack + index;
  }
  *vm.stackTop = value;
  vm.stackTop++;
}

Value pop() {
  vm.stackTop--;
  return *vm.stackTop;
}

static InterpretResult run() {
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
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
#endif
  uint8_t instruction;
  switch (instruction = READ_BYTE()) {
  case OP_CONSTANT: {
    Value constant = READ_CONSTANT();
    push(constant);
    break;
  }
  case OP_ADD:      BINARY_OP(+); break;
  case OP_SUBTRACT: BINARY_OP(-); break;
  case OP_MULTIPLY: BINARY_OP(*); break;
  case OP_DIVIDE:   BINARY_OP(/); break;
  case OP_NEGATE:   {
    Value constant = *(vm.stackTop - 1);
    *(vm.stackTop - 1) = -constant;
    break;
  }
  case OP_RETURN: {
    printValue(pop());
    printf("\n");
    return INTERPRET_OK;
  }
  }
}

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult interpret(const char* source) {
  Chunk chunk;
  initChunk(&chunk);

  if (!compile(source, &chunk)) {
    freeChunk(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  vm.chunk = &chunk;
  vm.ip = vm.chunk->code;

  InterpretResult result = run();

  freeChunk(&chunk);
  return result;
}
