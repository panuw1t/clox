#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "memory.h"
#include "vm.h"


int main(int argc, char *argv[])
{
#ifdef USE_MY_ALLOCATOR
  init_my_heap(1024 * 1024); // 1 MB
#endif
  initVM();
  Chunk chunk;
  initChunk(&chunk);

  int constant = addConstant(&chunk, 4.0);
  writeChunk(&chunk, OP_CONSTANT, 1);
  writeChunk(&chunk, constant, 1);

  constant = addConstant(&chunk, 3.0);
  writeChunk(&chunk, OP_CONSTANT, 1);
  writeChunk(&chunk, constant, 1);

  writeChunk(&chunk, OP_NEGATE, 1);

  constant = addConstant(&chunk, 2.0);
  writeChunk(&chunk, OP_CONSTANT, 1);
  writeChunk(&chunk, constant, 1);

  writeChunk(&chunk, OP_NEGATE, 1);
  writeChunk(&chunk, OP_MULTIPLY, 1);
  writeChunk(&chunk, OP_ADD, 1);


  writeChunk(&chunk, OP_RETURN, 1);
  /* disassembleChunk(&chunk, "test chunk"); */
  interpret(&chunk);
  freeVM();
  freeChunk(&chunk);
  return 0;
}
