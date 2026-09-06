#ifndef COMPILE_H
#define COMPILE_H

#include "object.h"
#include "vm.h"

ObjFunction* compile(const char* source);

#endif
