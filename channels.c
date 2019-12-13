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
