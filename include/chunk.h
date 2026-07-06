#ifndef CHUNK_H
#define CHUNK_H

#include "common.h"
#include "value.h"

typedef enum {
  OP_CONSTANT,
  OP_CONSTANT_SHORT,
  OP_RETURN,
} OpCode;

typedef struct {
  int count;
  int line;
} LineRecord;

typedef struct {
  int count;
  int capacity;
  LineRecord* record;
} LineArray;

typedef struct {
  int count;
  int capacity;
  uint8_t* code;
  ValueArray constants;
  LineArray lines;
} Chunk;

void initChunk(Chunk* chunk);
void freeChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte, int line);
int addConstant(Chunk* chunk, Value value);
void writeConstant(Chunk* chunk, Value value, int line);

void initLineArray(LineArray *lines);
void writeLine(LineArray* lines, int line);
void freeLineArray(LineArray* lines);
int getLine(LineArray* lines, int offset);

#endif
