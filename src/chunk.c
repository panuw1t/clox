#include <stdlib.h>

#include "memory.h"
#include "chunk.h"

void initChunk(Chunk* chunk) {
  chunk->count = 0;
  chunk->capacity = 0;
  chunk->code = NULL;
  initValueArray(&chunk->constants);
  initLineArray(&chunk->lines);
}

void freeChunk(Chunk* chunk) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
  initChunk(chunk);
  freeValueArray(&chunk->constants);
  freeLineArray(&chunk->lines);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line) {
  if (chunk->capacity < chunk->count + 1) {
    int oldCapacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(oldCapacity);
    chunk->code = GROW_ARRAY(uint8_t, chunk->code, oldCapacity, chunk->capacity);
  }
  writeLine(&chunk->lines, line);

  chunk->code[chunk->count] = byte;
  chunk->count++;
}

void writeLine(LineArray* lines, int line) {
  if (lines->capacity < lines->count + 1) {
    int oldCapacity = lines->capacity;
    lines->capacity = GROW_CAPACITY(oldCapacity);
    lines->record = GROW_ARRAY(LineRecord, lines->record, oldCapacity, lines->capacity);
  }

  if (lines->count > 1 &&
      lines->record[lines->count - 1].line == line) {
    lines->record[lines->count - 1].count++;
  } else {
    lines->record[lines->count].line = line;
    lines->record[lines->count].count = 1;
    lines->count++;
  }
}

int addConstant(Chunk* chunk, Value value) {
  writeValueArray(&chunk->constants, value);
  return chunk->constants.count - 1;
}


void initLineArray(LineArray *lines) {
  lines->record = NULL;
  lines->count = 0;
  lines->capacity = 0;
}

void freeLineArray(LineArray* lines) {
  FREE_ARRAY(LineRecord, lines->record, lines->capacity);
  initLineArray(lines);
}

int getLine(LineArray* lines, int offset) {
  int current = 0;
  for (int i = 0; i < lines->count; i++) {
    current += lines->record[i].count;
    if (offset < current) {
      return lines->record[i].line;
    }
  }
}
