#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h> /* access() */
#include <fcntl.h> /* open() */
#include <string.h> /* strlen() */
#include <unistd.h> /* write(), close() */
#include <limits.h> /* PIPE_BUF */
#include <pthread.h>

#define PIPE_BUF_SIZE PIPE_BUF

typedef struct pipe_conf_data {
    char *tx_pipe_name;
    char *rx_pipe_name;
    int is_client;
    int rx_thread_running;
    int (*pipe_receive_cb)(char *, int);
    pthread_t rx_tid;
    pthread_mutex_t rx_mutex;
    char pipe_rx_buf[PIPE_BUF_SIZE];
} pipe_conf_data_t;

static char *server_tx_pipe_name = "srv_receive_fifo";
static char *server_rx_pipe_name = "srv_send_fifo";

static char *client_tx_pipe_name = "cli_receive_fifo";
static char *client_rx_pipe_name = "cli_send_fifo";

static pipe_conf_data_t g_pipe_conf_data;

static void pipe_rx_process(char *buf, int len);

static int pipe_create(char *name)
{
    int ret;

    if (access(name, F_OK) == -1) {
        ret = mkfifo(name, 0664);
        if (ret != 0) {
            printf("main: mkfifo %s failed\n", name);
        }
    }

    return ret;
}

static void *pipe_rx_thread(void *data)
{
    pipe_conf_data_t *conf_data = (pipe_conf_data_t *)data;
    char *name = conf_data->rx_pipe_name;
    int pipe_fd;
    int len;
    char *quit_cmd = "rx_quit";
    char *buf = conf_data->pipe_rx_buf;

    while (conf_data->rx_thread_running) {
        pipe_fd = open(name, O_RDONLY);
        pthread_mutex_lock(&conf_data->rx_mutex);
        len = read(pipe_fd, buf, PIPE_BUF);
        printf("pipe read data: (%d) %s\n", len, buf);

        if(!strncmp(buf, quit_cmd, strlen(quit_cmd)))
            conf_data->rx_thread_running = 0;

        if (conf_data->pipe_receive_cb)
            conf_data->pipe_receive_cb(buf, len);
        pthread_mutex_unlock(&conf_data->rx_mutex);
        close(pipe_fd);
    }
    printf("%s():exit\n", __func__);
}

int pipe_init(int is_client)
{
    int ret;
    pipe_conf_data_t *conf_data = &g_pipe_conf_data;

    conf_data->pipe_receive_cb = NULL;
    conf_data->tx_pipe_name = server_tx_pipe_name;
    conf_data->rx_pipe_name = server_rx_pipe_name;

    if (is_client) {
        conf_data->tx_pipe_name = client_tx_pipe_name;
        conf_data->rx_pipe_name = client_rx_pipe_name;
    }
    conf_data->is_client = is_client;

    ret = pipe_create(conf_data->tx_pipe_name);
    if (ret != 0)
        return -1;
    ret = pipe_create(conf_data->rx_pipe_name);
    if (ret != 0) {
        unlink(conf_data->tx_pipe_name);
        return -1;
    }

    pthread_mutex_init(&conf_data->rx_mutex, NULL);

    conf_data->rx_thread_running = 1;
    ret = pthread_create(&conf_data->rx_tid, NULL, pipe_rx_thread, conf_data);
    if(ret != 0) {
        printf("%s: pipe rx thread creation failed!\n", __func__);
        return -1;
    }

    return 0;
}

void pipe_deinit()
{
    pipe_conf_data_t *conf_data = &g_pipe_conf_data;

    /* pipe_rx_thread exit */
    conf_data->rx_thread_running = 0;

    pthread_mutex_destroy(&conf_data->rx_mutex);

    /* remove fifo */
    unlink(conf_data->tx_pipe_name);
    unlink(conf_data->rx_pipe_name);
}

int register_pipe_receive_cb(int (*cb)(char *, int))
{
    pipe_conf_data_t *conf_data = &g_pipe_conf_data;

    if (cb) {
        conf_data->pipe_receive_cb = cb;
        return 0;
    } else
        return -1;
}

int pipe_msg_send(char *buf, int len)
{
    int pipe_fd;
    pipe_conf_data_t *conf_data = &g_pipe_conf_data;

    pipe_fd = open(conf_data->tx_pipe_name, O_WRONLY);
    printf("%s: (%d) %s\n", __func__, len, buf);
    write(pipe_fd, buf, len);
    close(pipe_fd);
}

