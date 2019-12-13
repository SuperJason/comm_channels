#include "channels.h"

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
