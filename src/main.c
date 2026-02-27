#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

int main(int argc, char *argv[])
{
  initVM();
  Chunk chunk;
  initChunk(&chunk);
  /* int constant = addConstant(&chunk, 1.2); */
  /* writeChunk(&chunk, OP_CONSTANT, 123); */
  /* writeChunk(&chunk, constant, 123); */
  /* constant = addConstant(&chunk, 3.4); */
  /* writeChunk(&chunk, OP_CONSTANT, 123); */
  /* writeChunk(&chunk, constant, 123); */
  /*  */
  /* writeChunk(&chunk, OP_ADD, 123); */
  /*  */
  /* constant = addConstant(&chunk, 5.6); */
  /* writeChunk(&chunk, OP_CONSTANT, 123); */
  /* writeChunk(&chunk, constant, 123); */
  /*  */
  /* writeChunk(&chunk, OP_DIVIDE, 123); */
  /* writeChunk(&chunk, OP_NEGATE, 123); */
  /*  */
  /* writeChunk(&chunk, OP_RETURN, 123); */

  writeConstant(&chunk, 4, 1);
  writeConstant(&chunk, 3, 1);
  writeConstant(&chunk, 2, 1);
  writeChunk(&chunk, OP_MULTIPLY, 1);
  writeChunk(&chunk, OP_ADD, 1);
  writeChunk(&chunk, OP_NEGATE, 1);
  writeChunk(&chunk, OP_RETURN, 2);


  /* disassembleChunk(&chunk, "test chunk"); */
  interpret(&chunk);
  freeVM();
  freeChunk(&chunk);
  return 0;
}
