#include "bee.h"

#include <stdio.h>

int main(int argc, char **argv){
  if(argc < 2){
    printf("missing file name\naborting\n");
    return 1;
  }
  return bee_run(argv[1]);
}
