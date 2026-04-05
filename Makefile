CC      = gcc
CFLAGS  = -Wall -Wextra -g -std=c11
LIBS    = -lpthread

.PHONY: all clean server client

all: server client

server: server.c
	$(CC) $(CFLAGS) -o server server.c $(LIBS)

client: client.c
	$(CC) $(CFLAGS) -o client client.c $(LIBS)

clean:
	rm -f server client