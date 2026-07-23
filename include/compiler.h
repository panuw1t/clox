#ifndef COMPILE_H
#define COMPILE_H

#include "object.h"
#include "vm.h"

bool compile(const char* source, Chunk* chunk);

#endif
