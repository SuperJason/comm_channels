CC := gcc

all: tcp_client tcp_server udp_client udp_server channels

channels: channels.c
	$(CC) $< -o $@

tcp_client: tcp_client.c
	$(CC) $< -o $@

tcp_server: tcp_server.c
	$(CC) $< -o $@

udp_client: udp_client.c
	$(CC) $< -o $@

udp_server: udp_server.c
	$(CC) $< -o $@

clean:
	@rm -f tcp_client tcp_server udp_client udp_server channels
