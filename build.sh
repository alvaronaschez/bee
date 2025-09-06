CC=cc
STD=c99
CFLAGS="-std=$STD -Wall -Wextra -pedantic"

if [ "$#" -eq 0 ] || [ "$1" = 'small' ]; then
	#echo small
	$CC $CFLAGS bee.c -o bee
elif [ "$1" = 'tiny' ]; then
	#echo tiny
	$CC $CFLAGS bee_tiny.c -o bee
elif [ "$1" = 'clean' ]; then
	rm bee
elif [ "$1" = 'build-debug' ]; then
	$CC $CFLAGS -g bee.c -o bee 
elif [ "$1" = 'debug' ]; then
	gdb -p $(pgrep bee)
fi
