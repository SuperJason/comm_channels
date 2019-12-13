#include "channels.h"

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

void process_msg_packet(uint8_t *data)
{
    uint8_t id = data[0];
    uint16_t len = data[2] | (data[3] << 8);
    uint8_t *p_u8 = data + 4;

    queue_packet(id, p_u8, len + 4);
    sem_post(&sem_packet_tx);
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
