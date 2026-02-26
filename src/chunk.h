#ifndef CHUNK_H
#define CHUNK_H

#include "common.h"
#include "value.h"


typedef enum {
  OP_CONSTANT,
  OP_RETURN,
  OP_CONSTANT_LONG,
} OpCode;

typedef struct {
  int line;
  int count;
} LineRun;

typedef struct {
  int count;
  int capacity;
  uint8_t* code;
  int lineCount;
  int lineCapacity;
  LineRun* lines;
  ValueArray constants;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
void writeLine(Chunk* chunk, int line);
int addConstant(Chunk* chunk, Value value);
void writeConstant(Chunk* chunk, Value value, int line);
int getLine(Chunk* chunk, int offset);

#endif
