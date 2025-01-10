CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -O2

.PHONY: all clean server client utils

all: server client

server: server.o utils.o
	$(CC) $(CFLAGS) -o server server.o utils.o

client: client.o utils.o
	$(CC) $(CFLAGS) -o client client.o utils.o

server.o: server.c
	$(CC) $(CFLAGS) -c server.c

client.o: client.c
	$(CC) $(CFLAGS) -c client.c

utils.o: utils.c
	$(CC) $(CFLAGS) -c utils.c

clean:
	rm -f server client *.o
