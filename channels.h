#ifndef CHANNELS_H__
#define CHANNELS_H__

#include <stdio.h>
#include <stdint.h> /* uint32_t */
#include <stdlib.h> /* malloc() */
#include <string.h> /* memcpy() */
#include "list.h"
#include <unistd.h>

/* tcp needed */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* thread */
#include<pthread.h>

/* pipo */
#include <sys/stat.h>
#include <fcntl.h>

/* sem_t */
#include <semaphore.h>

#define CHANNLS_DEBUG

#define CHANNEL_MAX 4
#define FRAME_LEN 32

#if FRAME_LEN > 255
#error "FRAME_LEN should be less than 255, since the length of frame is 1 byte"
#endif

/* channels */
typedef struct channel {
    uint32_t id;
    struct list_head tx_frames_head;
    struct list_head tx_packets_head;
} channel_t;

channel_t channels[CHANNEL_MAX];

/* low level */
typedef struct low_level_frame {
    struct list_head tx_frame;
    uint16_t length;
    uint8_t *data;
} low_level_frame_t;

uint8_t frame_buffer[FRAME_LEN];
char resp_buff[4096];

#ifdef CHANNLS_DEBUG
void frame_dump(uint8_t *data);
void hex_dump(uint8_t *data, int len);
#endif /* CHANNLS_DEBUG */
void send_frames(int id);
void queue_frames(int id);
int frame_check(uint8_t *data, int32_t length);

/* interface */
typedef struct channel_packet {
    struct list_head tx_packet;
    uint16_t length;
    uint8_t res[2];
    uint8_t *data;
} channel_packet_t;
uint8_t rx_packet_buff[4096];
int rx_packet_len;
channel_packet_t *new_packet(uint8_t *data, uint32_t len);
int queue_packet(int id, uint8_t *data, uint32_t len);
int msg_packet_check(uint8_t *data, int32_t length);
void process_msg_packet(uint8_t *data);
void process_packet_in_queue();
void channels_packet_show();
int process_rx_frame(uint8_t *data, int32_t length);
void process_rx_packet(uint8_t *data, int len);

/* crc */
uint32_t crc32(uint8_t *buf, uint32_t len);

/* pipe */

/* tcp */
#define TCP_COMM_PORT 8000
int server_sockfd;
int client_sockfd;
in_addr_t g_sock_addr;
signed short g_sock_port;
char rx_buff[4096];
void *tcp_thread_rx(void *data);
void *tcp_thread_tx(void *data);

/* other */
sem_t sem_packet_tx;
sem_t sem_packet_rx;
sem_t sem_frame_tx;
int response_status;
int is_client;

#endif /* CHANNELS_H__ */
