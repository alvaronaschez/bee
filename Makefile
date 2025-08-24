build b:
	cc -std=c11 -Wall -Wextra -Werror -pedantic main.c -o bee
run r: build
	@./bee /home/alvaro/ws/bee/main.c
clean c:
	rm bee
