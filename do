#!/bin/sh

# set -x # echo on

CC=cc
STD=c99
CFLAGS="-std=$STD -Wall -Wextra -pedantic -D_XOPEN_SOURCE -D_DEFAULT_SOURCE"

SOURCES="text.c text_util.c bee.c print.c"

build()
{
  for f in $SOURCES; do
    $CC $CFLAGS -c $f -o "obj/${f%.c}.o"
  done
  $CC $CFLAGS main.c obj/*.o -o bee
}

build_debug()
{
  CFLAGS="$CFLAGS -g3"
  build
}

clean()
{
  rm bee
  rm obj/*
  rm out/*
}

debug()
{
  gdb -tui -p $(pgrep bee)
}

test()
{
  cc -c text.c -o obj/text.o
  cc test_text.c obj/*.o -o out/test_text
  ./out/test_text
}


if [ "$#" -eq 0 ] || [ "$1" = 'build' ]; then
  build
elif [ "$1" = 'build-debug' ]; then
  build_debug
elif [ "$1" = 'clean' ]; then
  clean
elif [ "$1" = 'debug' ]; then
  debug
elif [ "$1" = 'test' ]; then
  test
else
  echo ERROR: unknown argument
fi

