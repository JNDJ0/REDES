CC = gcc

CFLAGS = -g

HDRS = common.c

all: server sensor

server: server.c $(HDRS)
	$(CC) $(CFLAGS) -o server server.c

sensor: sensor.c $(HDRS)
	$(CC) $(CFLAGS) -o sensor sensor.c

clean:
	rm -f server sensor *.o

.PHONY: all clean