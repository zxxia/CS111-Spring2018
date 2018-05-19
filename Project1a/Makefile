lab1a: lab1a.c
	gcc -o lab1a lab1a.c -std=c11 -g -Wall -Werror -Wextra -Wformat
clean:
	rm lab1a lab1a-123456789.tar.gz
check:
	./P1A_check.sh 123456789
dist:
	tar czvf lab1a-123456789.tar.gz lab1a.c README Makefile
