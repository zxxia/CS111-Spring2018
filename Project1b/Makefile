CC=gcc
CFLAGS= -g -Wall -Werror -Wextra -Wformat
lab1b-client: lab1b-client.c
	$(CC) -o $@ lab1b-client.c $(CFLAGS) 
lab1b-server: lab1b-server.c
	$(CC) -o $@ lab1b-server.c $(CFLAGS) 
clean:
	rm lab1b-client lab1b-server lab1b-123456789.tar.gz
check:
	./P1A_check.sh 123456789
dist:
	tar czvf lab1b-123456789.tar.gz lab1b-client.c lab1b-server.c README Makefile
