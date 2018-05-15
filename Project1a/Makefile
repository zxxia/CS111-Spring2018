lab1: main.c
	gcc -o lab1a main.c -std=c11 -g -Wall -Werror -Wextra -Wformat
clean:
	rm lab1a lab1a-123456789.tar.gz
check:
	./P1A_check.sh 123456789
dist:
	tar czvf lab1a-123456789.tar.gz main.c README Makefile
