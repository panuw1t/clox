#ifndef CHUNK_H
#define CHUNK_H

#include "common.h"
#include "value.h"

typedef enum {
  OP_CONSTANT,
  OP_CONSTANT_SHORT,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_POP,
  OP_GET_LOCAL,
  OP_GET_LOCAL_SHORT,
  OP_SET_LOCAL,
  OP_SET_LOCAL_SHORT,
  OP_GET_GLOBAL,
  OP_GET_GLOBAL_SHORT,
  OP_DEFINE_GLOBAL,
  OP_DEFINE_GLOBAL_SHORT,
  OP_SET_GLOBAL,
  OP_SET_GLOBAL_SHORT,
  OP_EQUAL,
  OP_GREATER,
  OP_LESS,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  OP_NOT,
  OP_NEGATE,
  OP_PRINT,
  OP_JUMP_IF_FALSE,
  OP_JUMP,
  OP_LOOP,
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
int writeConstant(Chunk* chunk, Value value, int line);

void initLineArray(LineArray *lines);
void writeLine(LineArray* lines, int line);
void freeLineArray(LineArray* lines);
int getLine(LineArray* lines, int offset);

#endif
