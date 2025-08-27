CC = cc
STD = c11
CFLAGS = -std=$(STD) -Wall -Wextra -pedantic
build b:
	$(CC) $(CFLAGS) main.c -o bee
run r: build
	@./bee /home/alvaro/ws/bee/main.c
clean c:
	rm bee
