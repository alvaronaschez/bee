#!/bin/sh
CC=cc
STD=c99
CFLAGS="-std=$STD -Wall -Wextra -pedantic"

if [ "$#" -eq 0 ] || [ "$1" = 'build' ]; then
	#echo small
	$CC $CFLAGS bee.c -o bee
elif [ "$1" = 'clean' ]; then
	rm bee
elif [ "$1" = 'build-debug' ]; then
	$CC $CFLAGS -g3 bee.c -o bee 
elif [ "$1" = 'debug' ]; then
	gdb -p $(pgrep bee)
elif [ "$1" = 'test' ]; then
	./script/test.sh
fi

