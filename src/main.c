#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "memory.h"
#include "allocator.h"


int main(int argc, char *argv[])
{
  test();
  Chunk chunk;
  initChunk(&chunk);
  int constant = addConstant(&chunk, 1.2);
  writeChunk(&chunk, OP_CONSTANT, 123);
  writeChunk(&chunk, constant, 123);

  writeChunk(&chunk, OP_RETURN, 123);

  writeConstant(&chunk, 10.3, 144);
  writeConstant(&chunk, 11.3, 144);
  writeConstant(&chunk, 12.3, 145);

  disassembleChunk(&chunk, "test chunk");
  freeChunk(&chunk);
  return 0;
}
