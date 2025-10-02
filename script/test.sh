cd $PWD
cc -c src/text.c -o obj/text.o
cc -I $PWD/src test/test_text.c obj/*.o -o out/test_text
./out/test_text
