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

typedef struct channel {
    uint32_t id;
    struct list_head tx_frames_head;
    struct list_head tx_packets_head;
} channel_t;

channel_t channels[CHANNEL_MAX];

typedef struct low_level_frame {
    struct list_head tx_frame;
    uint16_t length;
    uint8_t *data;
} low_level_frame_t;

typedef struct channel_packet {
    struct list_head tx_packet;
    uint16_t length;
    uint8_t res[2];
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
    p->length = len - 4;
    p->data = (uint8_t *)(p + sizeof(channel_packet_t));
    memcpy(p->data, data, len);

    return p;
}

int queue_packet(int id, uint8_t *data, uint32_t len)
{
    channel_packet_t *new;

    if (id >= CHANNEL_MAX) {
        printf("### %s:%d channel id is invalid!", __func__, __LINE__);
        return -1;
    }

    new = new_packet(data, len);
    list_add_tail(&(new->tx_packet), &(channels[id].tx_packets_head));

    printf(" [DEBUG] %s:%d, channel: %d, len: %d, packet queue\n", __func__, __LINE__, id, len);
    return 0;
}

#define CRC32_POLYNOMIAL_REV 0xedb88320l
uint32_t crc32(uint8_t *buf, uint32_t len)
{
    uint32_t crc = 0xffffffff;
    uint32_t crc_temp;
    int j;

    while (len > 0) {
        crc_temp = (crc ^ *buf) & 0xff;
        for (j = 8; j > 0; j--) {
            if (crc_temp & 1)
                crc_temp = (crc_temp >> 1)^CRC32_POLYNOMIAL_REV;
            else
                crc_temp >>= 1;
        }
        crc = ((crc >> 8) & 0x00FFFFFFL)^crc_temp;
        len--;
        buf++;
    }

    return(crc);
}

/*
 * msg_packet:
 * +----+-----+-----+----------+-----+
 * | id | res | len |   data   | crc |
 * +----+-----+-----+----------+-----+
 * id  : 1 byte, channel id
 * res : 1 byte, reserved
 * len : 2 byte, data length
 * data: data content
 * crc : 4 byte, id + length + data's crc32
 * LSB
 *
 * return:
 *   0: valied msg packet
 *  -1: invalided msg packet
 */
int msg_packet_check(uint8_t *data, int32_t length)
{
    uint8_t id = data[0];
    uint16_t len = data[2] | (data[3] << 8);
    uint32_t crc32_val;

#ifdef CHANNLS_DEBUG
    printf(" [DEBUG] %s:%d id: %d, len: %d\n", __func__, __LINE__, id, len);
#endif
    if (id >= CHANNEL_MAX)
        return -1;

    if (length < len + 8)
        return -1;

    crc32_val = data[4 + len];
    crc32_val |= (data[4 + len + 1] <<  8);
    crc32_val |= (data[4 + len + 2] << 16);
    crc32_val |= (data[4 + len + 3] << 24);

#ifdef CHANNLS_DEBUG
    printf(" [DEBUG] %s:%d crc32_val: 0x%04x(0x%04x)\n", __func__, __LINE__, crc32_val, crc32(data, len + 4));
#endif
    if (crc32(data, len + 4) != crc32_val)
        return -1;

    return 0;
}

sem_t sem_packet_tx;
sem_t sem_frame_tx;
int response_status;

void process_msg_packet(uint8_t *data)
{
    uint8_t id = data[0];
    uint16_t len = data[2] | (data[3] << 8);
    uint8_t *p_u8 = data + 4;

    queue_packet(id, p_u8, len + 4);
    sem_post(&sem_packet_tx);
}

int server_sockfd;
int client_sockfd;

#ifdef CHANNLS_DEBUG
void frame_dump(uint8_t *data)
{
    int i, j;
    int len = data[1] + 2;
    char frame_print_buf[80];

    for (j = 0; j * 16 < len; j++) {
        memset(frame_print_buf, 0, 80);
        for (i = 0; (i < 16) && (j * 16 + i < len); i++) {
            snprintf(&frame_print_buf[i * 3], 4, " %02x", data[j * 16 + i]);
        }
        printf(" [DEBUG] frame dump(%d):%s\n", len, frame_print_buf);
    }
}
void hex_dump(uint8_t *data, int len)
{
    int i, j;
    char frame_print_buf[80];

    for (j = 0; j * 16 < len; j++) {
        memset(frame_print_buf, 0, 80);
        for (i = 0; (i < 16) && (j * 16 + i < len); i++) {
            snprintf(&frame_print_buf[i * 3], 4, " %02x", data[j * 16 + i]);
        }
        printf(" [DEBUG] hex dump(%d):%s\n", len, frame_print_buf);
    }
}
#endif
/*
 * low level frame format
 * +----+-----+-----------+
 * | id | len |   data    |
 * +----+-----+-----------+
 * id  : 1 byte, channel id
 * len : 1 byte, data length
 * data: data content
 *
 */
uint8_t frame_buffer[FRAME_LEN];
void send_frames(int id)
{
    int len;
    low_level_frame_t *pllf;
#ifdef CHANNLS_DEBUG
    int count = 0;
#endif

    list_for_each_entry(pllf, &(channels[id].tx_frames_head), tx_frame) {
        memset(frame_buffer, 0, FRAME_LEN);
        frame_buffer[0] = id;
        len = pllf->length;
        if (len > FRAME_LEN)
            len = FRAME_LEN;
        frame_buffer[1] = len;
        memcpy(&frame_buffer[2], pllf->data, len);
#ifdef CHANNLS_DEBUG
        printf("pllf->data: %p\n", pllf->data);
        hex_dump(pllf->data, len);
#endif
        send(client_sockfd, frame_buffer, len + 2, 0);
#ifdef CHANNLS_DEBUG
        count++;
        frame_dump(frame_buffer);
#endif
        sem_wait(&sem_frame_tx);
    }
#ifdef CHANNLS_DEBUG
    printf(" [DEBUG] %s:%d -- channel: %d, %d frames sent\n", __func__, __LINE__, id, count);
#endif
}
void queue_frames(int id)
{
    int remain, len;
    channel_packet_t *pp;
    low_level_frame_t *pllf;

    pp = list_first_entry(&channels[id].tx_packets_head, channel_packet_t, tx_packet);

#ifdef CHANNLS_DEBUG
    printf(" [DEBUG] %s:%d -- channel: %d, pp->length:%d \n", __func__, __LINE__, id, pp->length);
#endif
    remain = pp->length + 4;
    len = remain;
    if (len > FRAME_LEN)
        len = FRAME_LEN;
    pllf = malloc(sizeof(low_level_frame_t) + len);
    INIT_LIST_HEAD(&(pllf->tx_frame));
    pllf->length = len;
    pllf->data = (uint8_t *)(pllf + sizeof(low_level_frame_t));
    pllf->data[0] = id;
    pllf->data[1] = 0; /* reserve */
    pllf->data[2] = pp->length & 0xff;
    pllf->data[3] = (pp->length >> 8) & 0xff;
    memcpy(&pllf->data[4], pp->data, len - 4);
#ifdef CHANNLS_DEBUG
    printf("pllf->data: %p\n", pllf->data);
    hex_dump(pllf->data, len - 4);
#endif
    list_add_tail(&(pllf->tx_frame), &(channels[id].tx_frames_head));
    remain -= len - 4;
#ifdef CHANNLS_DEBUG
    printf(" [DEBUG] %s:%d -- channel: %d, len:%d frames queued\n", __func__, __LINE__, id, len);
#endif
    while(remain > 0) {
        len = remain;
        if (len > FRAME_LEN)
            len = FRAME_LEN;
        pllf = malloc(sizeof(low_level_frame_t) + len);
        INIT_LIST_HEAD(&(pllf->tx_frame));
        pllf->length = len;
        pllf->data = (uint8_t *)(pllf + sizeof(low_level_frame_t));
        memcpy(pllf->data, &pp->data[pp->length + 4 - remain], len);
#ifdef CHANNLS_DEBUG
        printf("pllf->data: %p\n", pllf->data);
        hex_dump(pllf->data, len);
#endif
        remain -= len;
        if (remain < 4) {
        }
        list_add_tail(&(pllf->tx_frame), &(channels[id].tx_frames_head));
#ifdef CHANNLS_DEBUG
        printf(" [DEBUG] %s:%d -- channel: %d, len:%d frames queued\n", __func__, __LINE__, id, len);
#endif
    }
}
void process_packet_in_queue()
{
    int id;
    for (id = 0; id < CHANNEL_MAX; id++) {
       if (!list_empty(&channels[id].tx_frames_head)) {
           send_frames(id);
       }
       if (!list_empty(&channels[id].tx_packets_head)) {
           queue_frames(id);
           send_frames(id);
       }
    }
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

in_addr_t g_sock_addr;
signed short g_sock_port;
/*
 * low level frame format
 * +----+-----+-----------+
 * | id | len |   data    |
 * +----+-----+-----------+
 * id  : 1 byte, channel id
 * len : 1 byte, data length
 * data: data content
 *
 * return:
 *   0 : valid frame
 *  -1 : invalide frame
 */
int frame_check(uint8_t *data, int32_t length)
{
    uint8_t id = data[0];
    uint16_t len = data[1];

#ifdef CHANNLS_DEBUG
    printf(" [DEBUG] %s:%d id: %d, len: %d\n", __func__, __LINE__, id, len);
#endif
    if (id >= CHANNEL_MAX)
        return -1;

    if (length != len + 2)
        return -1;

    return 0;
}

/*
 * packet:
 * +----+-----+-----+----------+-----+
 * | id | res | len |   data   | crc |
 * +----+-----+-----+----------+-----+
 * id  : 1 byte, channel id
 * res : 1 byte, reserved
 * len : 2 byte, data length
 * data: data content
 * crc : 4 byte, id + length + data's crc32
 * LSB
 *
 */
uint8_t rx_packet_buff[4096];
void process_rx_packet(uint8_t *data, int len)
{
    static int status = 0, rx_len = 0;
    int id, packet_length;
    uint32_t crc32_val;

    printf(" [DEBUG] %s:%d status: %d, rx_len: %d, len: %d\n", __func__, __LINE__, status, rx_len, len);
    switch(status) {
        case 0: /* No packet rx or packet rx has finished. */
            memset(rx_packet_buff, 0, 4096);
            memcpy(rx_packet_buff, data, len);
            id = rx_packet_buff[0];
            packet_length = rx_packet_buff[2] | (rx_packet_buff[3] << 8);
            rx_len = len - 4;
            if (packet_length > 4096/* MAX PACKET LEN */) {
                printf("%s: invalid packet length!\n", __func__);
                status = 0;
            } else {
                status = 1;
            }
            break;
        case 1: /* Packet rx is on going. */
            memcpy(&rx_packet_buff[rx_len + 4], data, len);
            packet_length = rx_packet_buff[2] | (rx_packet_buff[3] << 8);
            printf(" [DEBUG] %s:%d packet_length: %d\n", __func__, __LINE__, packet_length);
            if (rx_len + len >= packet_length + 4) {
                crc32_val = rx_packet_buff[packet_length + 4];
                crc32_val |= (rx_packet_buff[packet_length + 4 + 1] <<  8);
                crc32_val |= (rx_packet_buff[packet_length + 4 + 2] << 16);
                crc32_val |= (rx_packet_buff[packet_length + 4 + 3] << 24);
                if (crc32(rx_packet_buff, packet_length + 4) == crc32_val) {
#ifdef CHANNLS_DEBUG
                    printf(" [DEBUG] %s:%d length %d packet rx\n", __func__, __LINE__, packet_length);
#endif
                } else {
                    printf("%s: crc failed! crc32_val: 0x%04x, calc crc32: 0x%04x\n", __func__, crc32_val, crc32(rx_packet_buff, packet_length + 4));
                }
                status = 0;
            } else {
                rx_len += len;
            }
            break;
        default:
            break;
    }
}

char resp_buff[4096];
int process_rx_frame(uint8_t *data, int32_t length)
{
    uint8_t id = data[0];
    uint16_t len = data[1];

    if (len == 8 && !strncmp(&data[2], "resp:ok", 7)) {
        response_status = 0;
    } else if (len == 9 && !strncmp(&data[2], "resp:err", 8)) {
        response_status = 1;
    } else {
        process_rx_packet(&data[2], len);

        resp_buff[0] = id;
        resp_buff[1] = 8;
        strncpy(&resp_buff[2], "resp:ok", 7);
        send(client_sockfd, resp_buff, 8 +2, 0);
    }
    sem_post(&sem_frame_tx);

    return 0;
}

char rx_buff[4096];
void *tcp_thread_rx(void *data)
{
    int len;

    while((len = recv(client_sockfd, rx_buff, BUFSIZ, 0)) > 0) {
        rx_buff[len]='\0';
        printf("client rx(%d): %s\n", len, rx_buff);
        if (!frame_check(rx_buff, len)) {
#ifdef CHANNLS_DEBUG
            frame_dump(rx_buff);
#endif
            process_rx_frame(rx_buff, len);
        }
    }

client_out:
    close(client_sockfd);
}

int is_client;
void *tcp_thread_tx(void *data)
{
    struct sockaddr_in local_addr, remote_addr;
    int sin_size, len, ret;
    pthread_t tcp_tid_rx;

    if ((server_sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        perror("tcp socket created failed\n");
        goto tx_out;
    }

    memset(&local_addr, 0, sizeof(local_addr));
    memset(&remote_addr, 0, sizeof(remote_addr));

    if (is_client) {
        remote_addr.sin_family=AF_INET;
        remote_addr.sin_addr.s_addr=g_sock_addr;
        remote_addr.sin_port=htons(g_sock_port);

        client_sockfd = server_sockfd;
        if (connect(client_sockfd, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) < 0) {
            perror("client: connect failed");
            goto tx_out;
        }
        printf("client: connected to server\n");
        len = recv(client_sockfd, rx_buff, BUFSIZ, 0);
        /* Print welcome message */
        rx_buff[len] = '\0';
        printf("%s", rx_buff);
    } else {
        local_addr.sin_family=AF_INET;
        local_addr.sin_addr.s_addr=g_sock_addr;
        local_addr.sin_port=htons(g_sock_port);

        if (bind(server_sockfd, (struct sockaddr *)&local_addr, sizeof(struct sockaddr)) < 0) {
            perror("server: bind failed\n");
            goto tx_out;
        }

        listen(server_sockfd, 5);

        /* Wait for client connection request */
        client_sockfd = accept(server_sockfd, (struct sockaddr *)&remote_addr, &sin_size);
        if(client_sockfd < 0) {
            perror("server: accept failed\n");
            goto tx_out;
        }

        printf("server: accept client %s\n",inet_ntoa(remote_addr.sin_addr));
        /* Send welcome message */
        send(client_sockfd, "Welcome to my server\n", 21, 0);
    }

    ret = pthread_create(&tcp_tid_rx, NULL, tcp_thread_rx, NULL);
    if(ret != 0) {
        perror("tcp thread rx create failed\n");
        goto tx_out;
    }

    /* wait for tx semaphore */
    while(1) {
        sem_wait(&sem_packet_tx);
#ifdef CHANNLS_DEBUG
        printf(" [DEBUG] %s:%d process tx packet\n", __func__, __LINE__);
#endif
        process_packet_in_queue();
    }

tx_out:
    close(server_sockfd);
    if (!is_client)
        close(client_sockfd);
}

char *server_pipe_name = "server_channel_fifo";
char *client_pipe_name = "client_channel_fifo";

#define BUFFER_SIZE 4096
uint8_t buff[BUFFER_SIZE];
int main(int argc, char *argv[])
{
    int i;
    uint16_t *p_short;
    channel_packet_t *pp;
    pthread_t tcp_tid_tx;
    int ret = 0, len;

    char *pipe_name = server_pipe_name;
    int pipe_fd;

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
                pipe_name = client_pipe_name;
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

    /* init channels */
    channels_init();

    /* init buffer */
    for (i = 0; i < BUFFER_SIZE / sizeof(uint16_t); i++) {
        p_short = (uint16_t *)&buff[i * sizeof(uint16_t)];
        *p_short = i;
    }

    printf(" [DEBUG] %s:%d\n", __func__, __LINE__);

    /* pipo init */
    if (access(pipe_name, F_OK) == -1) {
        ret = mkfifo(pipe_name, 0664);
        if (ret != 0) {
            perror("main: mkfifo failed\n");
            return -1;
        }
    }

    sem_init(&sem_packet_tx, 0, 0);
    sem_init(&sem_frame_tx, 0, 0);

    ret = pthread_create(&tcp_tid_tx, NULL, tcp_thread_tx, NULL);
    if(ret != 0) {
        perror("tcp thread create");
        return -1;
    }

    while(1)
    {
        memset(buff, 0, BUFFER_SIZE);
        pipe_fd = open(pipe_name, O_RDONLY);
        len = read(pipe_fd, buff, BUFFER_SIZE);
        printf("pipe read data: (%d) %s\n", len, buff);
        if(!strncmp(buff, "quit", 4))
            break;
        if(!strncmp(buff, "show", 4))
            channels_packet_show();
        if (!msg_packet_check(buff, len)) {
            process_msg_packet(buff);
        }
        close(pipe_fd);
    }

    /* Remove fifo */
    unlink(pipe_name);
    sem_destroy(&sem_packet_tx);
    sem_destroy(&sem_frame_tx);
    return 0;
}
