#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

void initChunk(Chunk* chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
  chunk->lines = NULL;
  chunk->lineCount = 0;
  chunk->lineCapacity = 0;
  initValueArray(&chunk->constants);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line) {
  if (chunk->capacity < chunk->count + 1) {
    int oldCapacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCapacity);
    chunk->code = GROW_ARRAY(uint8_t, chunk->code,
                             oldCapacity, chunk->capacity);
  }
  writeLine(chunk, line);
  chunk->code[chunk->count] = byte;
  chunk->count++;
}

void freeChunk(Chunk* chunk) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
  FREE_ARRAY(int, chunk->lines, chunk->capacity);
  freeValueArray(&chunk->constants);
  initChunk(chunk);
}

int addConstant(Chunk* chunk, Value value) {
  writeValueArray(&chunk->constants, value);
  return chunk->constants.count - 1;
}

void writeConstant(Chunk* chunk, Value value, int line) {
  int index = addConstant(chunk, value);
  if (index > UINT8_MAX) {
    uint8_t byte1 = (index >> 16) & 0xff;
    uint8_t byte2 = (index >> 8) & 0xff;
    uint8_t byte3 = index & 0xff;
    writeChunk(chunk, OP_CONSTANT_LONG, line);
    writeChunk(chunk, byte1, line);
    writeChunk(chunk, byte2, line);
    writeChunk(chunk, byte3, line);
  }
  else {
    writeChunk(chunk, OP_CONSTANT, line);
    writeChunk(chunk, index, line);
  }
}

void writeLine(Chunk* chunk, int line) {
  if (chunk->lineCount == 0) {
    if (chunk->lineCapacity < 1) {
      int oldCapacity = chunk->lineCapacity;
      chunk->lineCapacity = GROW_CAPACITY(oldCapacity);
      chunk->lines = GROW_ARRAY(LineRun, chunk->lines,
                                oldCapacity, chunk->lineCapacity);
    }

    chunk->lines[0].line = line;
    chunk->lines[0].count = 1;
    chunk->lineCount = 1;
    return;
  }

  LineRun* last = &chunk->lines[chunk->lineCount - 1];

  if (last->line == line) {
    last->count++;
    return;
  }

  if (chunk->lineCapacity < chunk->lineCount + 1) {
    int oldCapacity = chunk->lineCapacity;
    chunk->lineCapacity = GROW_CAPACITY(oldCapacity);
    chunk->lines = GROW_ARRAY(LineRun, chunk->lines,
                              oldCapacity, chunk->lineCapacity);
  }

  chunk->lines[chunk->lineCount].line = line;
  chunk->lines[chunk->lineCount].count = 1;
  chunk->lineCount++;
}

int getLine(Chunk* chunk, int offset) {
  int range = 0;
  for (int i = 0; i < chunk->lineCount; i++) {
    range += chunk->lines[i].count;
    if (offset < range)
      return chunk->lines[i].line;
  }
  return 0;
}
