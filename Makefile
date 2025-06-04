CC = gcc

CFLAGS = -g

HDRS = common2.c

all: server sensor

server: server2.c $(HDRS)
	$(CC) $(CFLAGS) -o server server2.c

sensor: sensor.c $(HDRS)
	$(CC) $(CFLAGS) -o sensor sensor.c

clean:
	rm -f server sensor *.o

.PHONY: all clean