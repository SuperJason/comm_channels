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

#include<pthread.h>

#define CHANNEL_MAX 4

typedef struct channel {
    uint32_t id;
    struct list_head tx_frames_head;
    struct list_head tx_packets_head;
} channel_t;

channel_t channels[CHANNEL_MAX];

typedef struct low_level_frame {
    uint16_t channel_id;
    uint16_t length;
    uint8_t *data;
} low_level_frame_t;

typedef struct channel_packet {
    struct list_head tx_packet;
    uint16_t length;
    uint8_t res[2];
    uint32_t crc;
    uint8_t *data;
} channel_packet_t;

int channels_init()
{
    int i;

    for(i = 0; i < CHANNEL_MAX; i++) {
        channels[i].id = i;
        INIT_LIST_HEAD(&(channels[i].tx_frames_head));
        INIT_LIST_HEAD(&(channels[i].tx_packets_head));
    }
}

channel_packet_t *new_packet(uint8_t *data, uint32_t len)
{
    channel_packet_t *p = malloc(sizeof(channel_packet_t) + len);

    if (p == NULL) {
        printf("### %s:%d packet malloc failed!", __func__, __LINE__);
        return NULL;
    }

    INIT_LIST_HEAD(&(p->tx_packet));
    p->length = len;
    p->crc = 0;
    p->data = (uint8_t *)(p + sizeof(channel_packet_t));
    memcpy(p->data, data, len);

    return p;
}

int queue_packet(int id, uint8_t *data, uint32_t len)
{
    channel_packet_t *pp, *new;

    if (id >= CHANNEL_MAX) {
        printf("### %s:%d channel id is invalid!", __func__, __LINE__);
        return -1;
    }

    new = new_packet(data, len);
    list_add_tail(&new->tx_packet, &(channels[id].tx_packets_head));

    return 0;
}

void channels_packet_show()
{
    int ch;
    channel_packet_t *pp;

    for (ch = 0; ch < CHANNEL_MAX; ch++) {
        printf(" ----- ch: %d\n", ch);
        list_for_each_entry(pp, &(channels[ch].tx_packets_head), tx_packet) {
            printf("pp->length is:%d\n", pp->length);
        }
    }
}

#define TCP_COMM_PORT 8000

int server_sockfd;
int client_sockfd;

in_addr_t g_sock_addr;
signed short g_sock_port;

char rx_buff[4096];
void *tcp_thread_client()
{
    int len;
    struct sockaddr_in remote_addr;

    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family=AF_INET;
    remote_addr.sin_addr.s_addr=g_sock_addr;
    remote_addr.sin_port=htons(g_sock_port);

    if ((client_sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("client: socket create failed\n");
        goto client_out;
    }

    if (connect(client_sockfd, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) < 0) {
        perror("client: connect failed");
        goto client_out;
    }

    printf("client: connected to server\n");
    len = recv(client_sockfd, rx_buff, BUFSIZ, 0);
    /* Print welcome message */
    rx_buff[len] = '\0';
    printf("%s", rx_buff);

    while((len = recv(client_sockfd, rx_buff, BUFSIZ, 0)) > 0) {
        rx_buff[len]='\0';
        printf("client rx: %s\n", rx_buff);
    }

client_out:
    close(client_sockfd);
}

void *tcp_thread_server()
{
    struct sockaddr_in local_addr;
    struct sockaddr_in remote_addr;
    int sin_size;
    int len;

    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family=AF_INET;
    local_addr.sin_addr.s_addr=g_sock_addr;
    local_addr.sin_port=htons(g_sock_port);

    if ((server_sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("server: socket created failed\n");
        goto server_out;
    }

    if (bind(server_sockfd, (struct sockaddr *)&local_addr, sizeof(struct sockaddr)) < 0) {
        perror("server: bind failed\n");
        goto server_out;
    }

    listen(server_sockfd, 5);

    /* Wait for client connection request */
    client_sockfd = accept(server_sockfd, (struct sockaddr *)&remote_addr, &sin_size);
    if(client_sockfd < 0) {
        perror("server: accept failed\n");
        goto server_out;
    }

    printf("server: accept client %s\n",inet_ntoa(remote_addr.sin_addr));
    /* Send welcome message */
    send(client_sockfd, "Welcome to my server\n", 21, 0);

    while((len = recv(client_sockfd, rx_buff, BUFSIZ, 0)) > 0) {
        rx_buff[len]='\0';
        printf("server rx: %s\n", rx_buff);
    }

server_out:
    close(client_sockfd);
    close(server_sockfd);
}

#define BUFFER_SIZE 4096
uint8_t buff[BUFFER_SIZE];
int main(int argc, char *argv[])
{
    int i, is_client = 0;
    uint16_t *p_short;
    channel_packet_t *pp;
    pthread_t tcp_tid;
    int ret = 0;

    g_sock_addr = INADDR_ANY;
    g_sock_port = TCP_COMM_PORT;

    while((i = getopt(argc, argv, "ca:p:")) != -1) {
        switch(i) {
            case 'a':
                printf("ipaddr: %s\n", optarg);
                g_sock_addr = inet_addr(optarg);
                break;
            case 'p':
                printf("port: %s\n", optarg);
                g_sock_port = atol(optarg);
                break;
            case 'c':
                is_client = 1;
                printf("socket connecting as client!\n");
                break;
            case '?':
                printf("unknown option\n");
                break;
            default:
                printf("default \n");
        }
    }


    printf("This is a sample code for communication!\n");
    printf(" [DEB] %s:%d\n", __func__, __LINE__);

    /* init channels */
    channels_init();

    /* init buffer */
    for (i = 0; i < BUFFER_SIZE / sizeof(uint16_t); i++) {
        p_short = (uint16_t *)&buff[i * sizeof(uint16_t)];
        *p_short = i;
    }

    /* Debug */
    queue_packet(0, &buff[0], 10);
    queue_packet(0, &buff[0], 11);
    queue_packet(1, &buff[1024], 20);
    queue_packet(2, &buff[2048], 30);
    queue_packet(3, &buff[256], 40);
    queue_packet(0, &buff[0], 12);
    queue_packet(2, &buff[2048], 31);
    queue_packet(0, &buff[0], 13);
    queue_packet(1, &buff[2048], 21);
    queue_packet(2, &buff[2048], 32);

    channels_packet_show();

    if (is_client) {/* Works as client */
        ret = pthread_create(&tcp_tid, NULL, tcp_thread_client, NULL);
    } else {/* By default works as server */
        ret = pthread_create(&tcp_tid, NULL, tcp_thread_server, NULL);
    }
    if(ret != 0) {
        perror("tcp thread create");
        return -1;
    }

    while(1)
    {
        memset(buff, 0, BUFFER_SIZE);
        printf("main -- enter string to send: \n");
        scanf("%s", buff);
        if(!strcmp(buff,"quit"))
            break;
        send(client_sockfd, buff, strlen(buff), 0);
    }

    return 0;
}
