#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(int argc, char *argv[])
{
  Chunk chunk;
  initChunk(&chunk);
  writeChunk(&chunk, OP_RETURN, 2);
  writeChunk(&chunk, OP_RETURN, 2);
  writeChunk(&chunk, OP_RETURN, 2);
  writeChunk(&chunk, OP_RETURN, 3);
  writeChunk(&chunk, OP_RETURN, 4);
  writeChunk(&chunk, OP_RETURN, 10);
  writeConstant(&chunk, 1.2, 11);
  writeConstant(&chunk, 1.3, 11);
  writeConstant(&chunk, 1.4, 12);
  writeConstant(&chunk, 1.5, 13);


  disassembleChunk(&chunk, "test chunk");
  freeChunk(&chunk);
  return 0;
}
