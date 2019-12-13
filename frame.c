#include "channels.h"

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
#endif /* CHANNLS_DEBUG */

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
