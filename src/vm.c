#include <stdarg.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"
#include "compiler.h"

VM vm;

static void resetStack() {
  vm.stackTop = vm.stack;
  vm.frameCount = 0;
}

static void runtimeError(const char* format, ...) {
  va_list args;
  va_start(args, format);
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  for (int i = vm.frameCount - 1; i >= 0; i--) {
    CallFrame* frame = &vm.frames[i];
    ObjFunction* function = frame->function;
    size_t instruction = frame->ip - function->chunk.code - 1;
    int line = getLine(&frame->function->chunk.lines, instruction);
    fprintf(stderr, "[line %d] in ", line);
    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }

  resetStack();
}

void initStack() {
  vm.stackCapacity = 0;
  vm.stack = NULL;
  vm.stackTop = NULL;
}

void initVM() {
  initStack();
  initTable(&vm.strings);
  initValueArray(&vm.globals);
  initTable(&vm.globalIndices);
  vm.objects = NULL;
  vm.frameCount = 0;
}

void freeVM() {
  freeObjects();
  freeTable(&vm.strings);
  freeValueArray(&vm.globals);
  freeTable(&vm.globalIndices);
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

static bool isFalsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate() {
  ObjString* b = AS_STRING(pop());
  ObjString* a = AS_STRING(pop());

  int length = a->length + b->length;
  ObjString* string = allocateString(length, 0);
  memcpy(string->chars, a->chars, a->length);
  memcpy(string->chars + a->length, b->chars, b->length);
  string->chars[length] = '\0';
  uint32_t hash = hashString(string->chars, length);
  string->hash = hash;
  ObjString* interned = tableFindString(&vm.strings, string->chars, length, hash);
  if (interned != NULL) {
    FREE(ObjString, string);
    push(OBJ_VAL(interned));
  } else {
    tableSet(&vm.strings, OBJ_VAL(string), NIL_VAL);
    push(OBJ_VAL(string));
  }
}

static Value peek(int distance) {
  return vm.stackTop[-1 - distance];
}

static bool call(ObjFunction* function, int argCount) {
  if (argCount != function->arity) {
    runtimeError("Expected %d arguments but got %d.", function->arity, argCount);
    return false;
  }

  if (vm.frameCount == FRAMES_MAX) {
    runtimeError("Stack overflow.");
    return false;
  }

  CallFrame* frame = &vm.frames[vm.frameCount++];
  frame->function = function;
  frame->ip = function->chunk.code;
  if (vm.stackTop == NULL) {
    frame->slots = vm.stackTop;
  } else {
    frame->slots = vm.stackTop - argCount - 1;
  }
  return true;
}

static bool callValue(Value callee, int argCount) {
  if (IS_OBJ(callee)) {
    switch (OBJ_TYPE(callee)) {
    case OBJ_FUNCTION:
      return call(AS_FUNCTION(callee), argCount);
    case OBJ_NATIVE: {
      NativeFn native = AS_NATIVE(callee);
      Value result = native(argCount, vm.stackTop - argCount);
      vm.stackTop -= argCount + 1;
      push(result);
      return true;
    }
    default:
      break; // Non-callable object type.
    }
  }
  runtimeError("Can only call functions and classes.");
  return false;
}

static InterpretResult run() {
  CallFrame* frame = &vm.frames[vm.frameCount - 1];
  uint8_t* ip = frame->ip;
#define READ_BYTE() (*ip++)
#define READ_SHORT() (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
#define READ_CONSTANT_SHORT() (frame->function->chunk.constants.values[READ_SHORT()])
#define READ_STRING() AS_STRING(READ_CONSTANT())
#define BINARY_OP(valueType, op) \
    do { \
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
        runtimeError("Operands must be numbers."); \
        return INTERPRET_RUNTIME_ERROR; \
      } \
      double b = AS_NUMBER(pop()); \
      double a = AS_NUMBER(pop()); \
      push(valueType(a op b)); \
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
  disassembleInstruction(&frame->function->chunk, (int)(ip - frame->function->chunk.code));
#endif
  uint8_t instruction;
  switch (instruction = READ_BYTE()) {
  case OP_CONSTANT_SHORT: {
    Value constant = READ_CONSTANT_SHORT();
    push(constant);
    break;
  }
  case OP_CONSTANT: {
    Value constant = READ_CONSTANT();
    push(constant);
    break;
  }
  case OP_NIL: push(NIL_VAL); break;
  case OP_TRUE: push(BOOL_VAL(true)); break;
  case OP_FALSE: push(BOOL_VAL(false)); break;
  case OP_POP: pop(); break;
  case OP_SET_LOCAL: {
    uint8_t slot = READ_BYTE();
    frame->slots[slot] = peek(0);
    break;
  }
  case OP_SET_LOCAL_SHORT: {
    uint16_t slot = READ_SHORT();
    frame->slots[slot] = peek(0);
    break;
  }
  case OP_GET_LOCAL: {
    uint8_t slot = READ_BYTE();
    push(frame->slots[slot]);
    break;
  }
  case OP_GET_LOCAL_SHORT: {
    uint16_t slot = READ_SHORT();
    push(frame->slots[slot]);
    break;
  }
  case OP_GET_GLOBAL: {
    uint8_t index = READ_BYTE();
    Value value = vm.globals.values[index];
    if (IS_UNDEFINE(value)) {
      runtimeError("Undefined variable '%s'.", getGlobalNameByIndex(&vm.globalIndices, index));
      return INTERPRET_RUNTIME_ERROR;
    }
    push(value);
    break;
  }
  case OP_GET_GLOBAL_SHORT: {
    uint16_t index = READ_SHORT();
    Value value = vm.globals.values[index];
    if (IS_UNDEFINE(value)) {
      runtimeError("Undefined variable '%s'.", getGlobalNameByIndex(&vm.globalIndices, index));
      return INTERPRET_RUNTIME_ERROR;
    }
    push(value);
    break;
  }
  case OP_DEFINE_GLOBAL: {
    uint8_t index = READ_BYTE();
    vm.globals.values[index] = pop();
    break;
  }
  case OP_DEFINE_GLOBAL_SHORT: {
    uint16_t index = READ_SHORT();
    vm.globals.values[index] = pop();
    break;
  }
  case OP_SET_GLOBAL: {
    uint16_t index = READ_BYTE();
    if (IS_UNDEFINE(vm.globals.values[index])) {
      runtimeError("Undefined variable '%s'.", getGlobalNameByIndex(&vm.globalIndices, index));
      return INTERPRET_RUNTIME_ERROR;
    }
    vm.globals.values[index] = peek(0);
    break;
  }
  case OP_SET_GLOBAL_SHORT: {
    uint16_t index = READ_SHORT();
    if (IS_UNDEFINE(vm.globals.values[index])) {
      runtimeError("Undefined variable '%s'.", getGlobalNameByIndex(&vm.globalIndices, index));
      return INTERPRET_RUNTIME_ERROR;
    }
    vm.globals.values[index] = peek(0);
    break;
  }
  case OP_EQUAL: {
    Value b = pop();
    Value a = pop();
    push(BOOL_VAL(valuesEqual(a, b)));
    break;
  }
  case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
  case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;
  case OP_ADD: {
    if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
      concatenate();
    } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
      double b = AS_NUMBER(pop());
      double a = AS_NUMBER(pop());
      push(NUMBER_VAL(a + b));
    } else {
      runtimeError("Operands must be two numbers or two strings.");
      return INTERPRET_RUNTIME_ERROR;
    }
    break;
  }
  case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
  case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
  case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
  case OP_NOT: push(BOOL_VAL(isFalsey(pop()))); break;
  case OP_NEGATE: {
    if (!IS_NUMBER(peek(0))) {
      runtimeError("Operand must be a number.");
      return INTERPRET_RUNTIME_ERROR;
    }
    push(NUMBER_VAL(-AS_NUMBER(pop())));
    break;
  }
  case OP_PRINT: {
    printValue(pop());
    printf("\n");
    break;
  }
  case OP_JUMP: {
    uint16_t offset = READ_SHORT();
    ip += offset;
    break;
  }
  case OP_JUMP_IF_FALSE: {
    uint16_t offset = READ_SHORT();
    if (isFalsey(peek(0))) ip += offset;
    break;
  }
  case OP_LOOP: {
    uint16_t offset = READ_SHORT();
    ip -= offset;
    break;
  }
  case OP_DUPE: push(peek(0)); break;
  case OP_CALL: {
    int argCount = READ_BYTE();
    frame->ip = ip;
    if (!callValue(peek(argCount), argCount)) {
      return INTERPRET_RUNTIME_ERROR;
    }
    frame = &vm.frames[vm.frameCount - 1];
    ip = frame->ip;
    break;
  }
  case OP_RETURN: {
    Value result = pop();
    vm.frameCount--;
    if (vm.frameCount == 0) {
      pop();
      return INTERPRET_OK;
    }

    vm.stackTop = frame->slots;
    push(result);
    frame = &vm.frames[vm.frameCount - 1];
    ip = frame->ip;
    break;
  }
  }
}

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_CONSTANT_SHORT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(const char* source) {
  ObjFunction* function = compile(source);
  if (function == NULL) return INTERPRET_COMPILE_ERROR;

  push(OBJ_VAL(function));
  call(function, 0);

  return run();
}
