#include <stdio.h>
#include <stdlib.h>

#include "allocator.h"
#include "util.h"
#include "vm.h"

int main(int argc, char *argv[]) {
  if (argc == 1) {
    test();
  } else if (argc == 2) {
    while (*argv != NULL) {
      printf("%s\n", *argv++);
    }
  }
  return 0;
}
