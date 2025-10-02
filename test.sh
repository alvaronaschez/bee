cc -c text.c -o obj/text.o
cc test_text.c obj/*.o -o out/test_text
./out/test_text
