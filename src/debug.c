#include <stdio.h>

#include "debug.h"
#include "value.h"

void disassembleChunk(Chunk* chunk, const char* name) {
  printf("== %s ==\n", name);

  for (int offset = 0; offset < chunk->count;) {
    offset = disassembleInstruction(chunk, offset);
  }
}

static int constantInstruction(const char* name, Chunk* chunk, int offset) {
  int constant = 0;
  int newOffset = 0;
  switch(chunk->code[offset]) {
  case OP_CONSTANT:
    constant = chunk->code[offset + 1];
    newOffset = offset + 2;
    break;
  case OP_CONSTANT_SHORT:
    constant = chunk->code[offset + 1] << 8;
    constant = constant + chunk->code[offset + 2];
    newOffset = offset + 3;
    break;
  }
  printf("%-16s %4d '", name, constant);
  printValue(chunk->constants.values[constant]);
  printf("'\n");
  return newOffset;
}

static int simpleInstruction(const char* name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

int disassembleInstruction(Chunk* chunk, int offset) {
  printf("%04d ", offset);
  int line = getLine(&chunk->lines, offset);
  int oldLine = getLine(&chunk->lines, offset - 1);
  if (offset > 0 && line == oldLine) {
    printf("   | ");
  } else {
    printf("%4d ", line);
  }

  uint8_t instruction = chunk->code[offset];
  switch (instruction) {
  case OP_CONSTANT:
    return constantInstruction("OP_CONSTANT", chunk, offset);
  case OP_CONSTANT_SHORT:
    return constantInstruction("OP_CONSTANT_SHORT", chunk, offset);
  case OP_RETURN:
    return simpleInstruction("OP_RETURN", offset);
  default:
    printf("Unknown opcode %d\n", instruction);
    return offset + 1;
  }
}
