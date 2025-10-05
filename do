#!/bin/sh
CC=cc
STD=c99
CFLAGS="-std=$STD -Wall -Wextra -pedantic"

if [ "$#" -eq 0 ] || [ "$1" = 'build' ]; then
  $CC $CFLAGSS -c text.c -o obj/text.o
  $CC $CFLAGS bee.c obj/*.o -o bee
elif [ "$1" = 'clean' ]; then
  rm bee
elif [ "$1" = 'build-debug' ]; then
  $CC $CFLAGSS -g3 -c text.c -o obj/text.o
  $CC $CFLAGS -g3 bee.c obj/*.o -o bee 
elif [ "$1" = 'debug' ]; then
  gdb -tui -p $(pgrep bee)
elif [ "$1" = 'test' ]; then
  cc -c text.c -o obj/text.o
  cc test_text.c obj/*.o -o out/test_text
  ./out/test_text
else
  echo ERROR: unknown argument
fi

