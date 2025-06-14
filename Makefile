CC = gcc

CFLAGS = -g

HDRS = common.c

all: server client

server: server.c $(HDRS)
	$(CC) $(CFLAGS) -o server server.c

client: client.c $(HDRS)
	$(CC) $(CFLAGS) -o client client.c

clean:
	rm -f server client *.o

.PHONY: all clean