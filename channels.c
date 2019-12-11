#include <stdio.h>
#include <stdint.h> /* uint32_t */
#include <stdlib.h> /* malloc() */
#include <string.h> /* memcpy() */
#include "list.h"

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

#define BUFFER_SIZE 4096
uint8_t buff[BUFFER_SIZE];
int main(int argc, char *argv[])
{
    int i;
    uint16_t *p_short;
    channel_packet_t *pp;

    printf("This is a sample code for communication!\n");
    printf(" [DEB] %s:%d\n", __func__, __LINE__);

    /* init channels */
    channels_init();

    /* init buffer */
    for (i = 0; i < BUFFER_SIZE / sizeof(uint16_t); i++) {
        p_short = (uint16_t *)&buff[i * sizeof(uint16_t)];
        *p_short = i;
    }

    queue_packet(0, &buff[0], 10);
    queue_packet(0, &buff[0], 11);
    queue_packet(1, &buff[1024], 20);
    queue_packet(2, &buff[2048], 30);
    queue_packet(3, &buff[256], 40);
    queue_packet(0, &buff[0], 12);

    channels_packet_show();

    return 0;
}
