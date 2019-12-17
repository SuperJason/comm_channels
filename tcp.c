#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h> /* close() */
#include <string.h> /* memset() */

/* #define DEBUG */
#define LOG_TAGS "TCP"
#include "log.h"

#define TCP_RX_BUF_SIZE BUFSIZ
typedef struct tcp_conf_data {
    int server_sockfd;
    int client_sockfd;
    int is_client;
    int rx_thread_running;
    pthread_t rx_tid;
    pthread_mutex_t rx_mutex;
    int (*tcp_receive_cb)(char *, int);
    char rx_buf[TCP_RX_BUF_SIZE];
} tcp_conf_data_t;

static tcp_conf_data_t g_tcp_conf_data;

void *tcp_thread_rx(void *data)
{
    int len;
    tcp_conf_data_t *conf_data = &g_tcp_conf_data;
    char *buf = conf_data->rx_buf;

    while(conf_data->rx_thread_running) {
        pthread_mutex_lock(&conf_data->rx_mutex);
        len = recv(conf_data->client_sockfd, buf, BUFSIZ, 0);
        buf[len]='\0';
        pr_debug("client rx(%d): %s\n", len, buf);

        if (conf_data->tcp_receive_cb)
            conf_data->tcp_receive_cb(buf, len);
        pthread_mutex_unlock(&conf_data->rx_mutex);
    }
    pr_notice("%s():exit\n", __func__);
}

int tcp_init(int is_client, in_addr_t sock_addr, signed short sock_port)
{
    struct sockaddr_in local_addr, remote_addr;
    tcp_conf_data_t *conf_data = &g_tcp_conf_data;
    int sin_size, len, ret;
    char *welcome_message = "Welcome to tcp communication server\n";

    if ((conf_data->server_sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        pr_err("%s: tcp socket created failed!\n", __func__);
        return -1;
    }

    memset(&local_addr, 0, sizeof(local_addr));
    memset(&remote_addr, 0, sizeof(remote_addr));

    if (is_client) {
        remote_addr.sin_family = AF_INET;
        remote_addr.sin_addr.s_addr = sock_addr;
        remote_addr.sin_port = htons(sock_port);

        conf_data->client_sockfd = conf_data->server_sockfd;
        if (connect(conf_data->client_sockfd, (struct sockaddr *)&remote_addr, sizeof(struct sockaddr)) < 0) {
            pr_err("%s: tcp client connect failed!\n", __func__);
            close(conf_data->client_sockfd);
            return -1;
        }
        pr_notice("client: connected to server\n");
        len = recv(conf_data->client_sockfd, conf_data->rx_buf, BUFSIZ, 0);

        /* Print welcome message */
        conf_data->rx_buf[len] = '\0';
        pr_notice("%s",  conf_data->rx_buf);
    } else {
        local_addr.sin_family = AF_INET;
        local_addr.sin_addr.s_addr = sock_addr;
        local_addr.sin_port = htons(sock_port);

        if (bind(conf_data->server_sockfd, (struct sockaddr *)&local_addr, sizeof(struct sockaddr)) < 0) {
            pr_err("%s: server bind failed!\n", __func__);
            close(conf_data->server_sockfd);
            return -1;
        }

        listen(conf_data->server_sockfd, 5);

        /* Wait for client connection request */
        conf_data->client_sockfd = accept(conf_data->server_sockfd, (struct sockaddr *)&remote_addr, &sin_size);
        if(conf_data->client_sockfd < 0) {
            pr_err("%s: server accept failed!\n", __func__);
            close(conf_data->server_sockfd);
            return -1;
        }

        pr_notice("server: accept client %s\n",inet_ntoa(remote_addr.sin_addr));
        /* Send welcome message */
        send(conf_data->client_sockfd, welcome_message, strlen(welcome_message), 0);
    }

    conf_data->is_client = is_client;
    pthread_mutex_init(&conf_data->rx_mutex, NULL);

    conf_data->rx_thread_running = 1;
    ret = pthread_create(&conf_data->rx_tid, NULL, tcp_thread_rx, NULL);
    if(ret != 0) {
        pr_err("%s: tcp thread rx create failed!\n", __func__);
        close(conf_data->server_sockfd);
        close(conf_data->client_sockfd);
        return -1;
    }

    return 0;
}

void tcp_deinit()
{
    tcp_conf_data_t *conf_data = &g_tcp_conf_data;

    /* tcp_rx_thread exit */
    conf_data->rx_thread_running = 0;

    pthread_mutex_destroy(&conf_data->rx_mutex);

    close(conf_data->server_sockfd);
    close(conf_data->client_sockfd);
}

int register_tcp_receive_cb(int (*cb)(char *, int))
{
    tcp_conf_data_t *conf_data = &g_tcp_conf_data;

    if (cb) {
        conf_data->tcp_receive_cb = cb;
        return 0;
    } else
        return -1;
}

int tcp_frame_send(char *buf, int len)
{
    tcp_conf_data_t *conf_data = &g_tcp_conf_data;

    pr_debug("%s: (%d) %s\n", __func__, len, buf);
    send(conf_data->client_sockfd, buf, len, 0);
}
