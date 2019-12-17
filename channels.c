#include "channels.h"

int channels_init()
{
    int i;

    for(i = 0; i < CHANNEL_MAX; i++) {
        channels[i].id = i;
        INIT_LIST_HEAD(&(channels[i].tx_frames_head));
        INIT_LIST_HEAD(&(channels[i].tx_packets_head));
    }
}

int main_loop_state = 1;

static int msg_receive(char *buf, int len)
{
    char *quit_cmd = "rx_quit";

    printf("%s: (%d) %s\n", __func__, len, buf);

    if(!strncmp(buf, quit_cmd, strlen(quit_cmd)))
        main_loop_state = 0;

    tcp_frame_send(buf, len);
}

static int frame_receive(char *buf, int len)
{
    printf("%s: (%d) %s\n", __func__, len, buf);
    pipe_msg_send(buf, len);
}

int main(int argc, char *argv[])
{
    int opt;
    int ret, len;
    in_addr_t sock_addr;
    signed short sock_port;

    sock_addr = INADDR_ANY;
    sock_port = TCP_COMM_PORT;

    while((opt = getopt(argc, argv, "ca:p:")) != -1) {
        switch(opt) {
            case 'a':
                printf("ipaddr: %s\n", optarg);
                sock_addr = inet_addr(optarg);
                break;
            case 'p':
                printf("port: %s\n", optarg);
                sock_port = atol(optarg);
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
    printf(" [DEBUG] %s:%d\n", __func__, __LINE__);

    /* pipe init */
    pipe_init(is_client);
    register_pipe_receive_cb(msg_receive);

    /* tcp connection init */
    tcp_init(is_client, sock_addr, sock_port);
    register_tcp_receive_cb(frame_receive);

    while (main_loop_state) {
    }

    printf("%s():exit\n", __func__);

    pipe_deinit();
    tcp_deinit();

    return 0;
}
