CC := gcc

all: channels

channels: channels.c crc32.c frame.c packet.c pipe.c tcp.c list.h channels.h
	$(CC) channels.c crc32.c frame.c packet.c pipe.c tcp.c -o $@ -lpthread

clean:
	@rm -f channels *.o
