#!/bin/sh

# set -x # echo on

kernel_name=$(uname -s)

STD=c99

if [ "$kernel_name" = "Linux" ]; then
  CC=gcc
  GDB=gdb
  CFLAGS="-std=$STD -Wall -Wextra -pedantic -D_XOPEN_SOURCE -D_DEFAULT_SOURCE"
  #echo $kernel_name
elif [ "$kernel_name" = "OpenBSD" ]; then
  CC=cc
  GDB=egdb
  CFLAGS="-std=$STD -Wall -Wextra -pedantic -D_XOPEN_SOURCE -D_BSD_SOURCE"
  #echo $kernel_name
elif [ "$kernel_name" = "FreeBSD" ]; then
  CC=cc
  GDB=gdb
  CFLAGS="-std=$STD -Wall -Wextra -pedantic -D_DEAFAULT_SOURCE -D_XOPEN_SOURCE"
  #echo $kernel_name
else
	echo unknown system
fi

SOURCES="bee.c text.c text_util.c file.c print.c"

help()
{
  echo help
  echo build
  echo build-debug
  echo clean
  echo debug
  echo test
  echo test-debug
  echo cloc
}

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
  $GDB -tui -p $(pgrep bee)
}

test()
{
  $CC -c text.c -o obj/text.o
  $CC test_text.c obj/*.o -o out/test_text
  ./out/test_text
}

test_debug()
{
  $CC -c -g3 text.c -o obj/text.o
  $CC -g3 test_text.c obj/*.o -o out/test_text
  $GDB -tui ./out/test_text
}


if [ "$#" -eq 0 ] || [ "$1" = 'help' ]; then
  help
elif [ "$1" = 'build' ]; then
  build
elif [ "$1" = 'build-debug' ]; then
  build_debug
elif [ "$1" = 'clean' ]; then
  clean
elif [ "$1" = 'debug' ]; then
  debug
elif [ "$1" = 'test' ]; then
  test
elif [ "$1" = 'test-debug' ]; then
  test_debug
elif [ "$1" = 'cloc' ]; then
  cloc $SOURCES main.c
else
  echo ERROR: unknown argument
fi

