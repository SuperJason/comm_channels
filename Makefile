CC := gcc

all: channels

channels: channels.c list.h
	$(CC) $< -o $@ -lpthread

clean:
	@rm -f channels
