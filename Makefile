CC = gcc
CFLAGS = -Wall -Wextra -Wpedantic -O2

.PHONY: all clean

all: server client

server: server.o utils.o
	$(CC) $(CFLAGS) -o $@ $^

client: client.o utils.o
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f server client *.o
