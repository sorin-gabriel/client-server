#include "includes.h"

int main(int argc, char const *argv[]) {

    EXIT_IF(argc != 4, "wrong number of arguments");

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    int retval, optval = 1;

    struct sockaddr_in srvr;
    srvr.sin_family = AF_INET;
    srvr.sin_port = htons(atoi(argv[3]));
    retval = inet_aton(argv[2], &srvr.sin_addr);
    EXIT_IF(retval == 0, "bad ip");

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    EXIT_IF(sockfd < 0, "dead socket");

    retval = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(int));
    EXIT_IF(retval < 0, "dead socket");

    retval = connect(sockfd, (struct sockaddr *)&srvr, sizeof(srvr));
    EXIT_IF(retval < 0, "no connection");

    fd_set read_fd_set, temp_fd_set;
    int maxfd;

    FD_ZERO(&read_fd_set);
    FD_SET(sockfd, &read_fd_set);
    FD_SET(STDIN_FILENO, &read_fd_set);

    maxfd =
        STDIN_FILENO >= sockfd ?
        STDIN_FILENO : sockfd;


    proto_pkt buffered_pkt = { 0 };

    strncpy(buffered_pkt.tcp_pkt.id, argv[1], ID_LEN);
    proto_send(sockfd, &buffered_pkt, sizeof(buffered_pkt.tcp_pkt));

    char *buff = (char *)malloc(PROTO_BUFF * sizeof(char));
    
    int shutdown = 0;
    while (!shutdown) {

        temp_fd_set = read_fd_set;
        retval = select(maxfd + 1, &temp_fd_set, NULL, NULL, NULL);
        EXIT_IF(retval < 0, "bad socket selection");

        if (FD_ISSET(STDIN_FILENO, &temp_fd_set)) {

            EXIT_IF(fgets(buff, PROTO_BUFF, stdin) == NULL, "bad input");

            if (buff[strlen(buff) - 1] == '\n') buff[strlen(buff) - 1] = '\0';

            if(!strncmp(buff, "exit", 4)) {

                strncpy(buffered_pkt.tcp_pkt.id, argv[1], ID_LEN);
                strcpy(buffered_pkt.tcp_pkt.intent, "exit");
                proto_send(sockfd, &buffered_pkt, sizeof(buffered_pkt.tcp_pkt));

                FD_ZERO(&temp_fd_set);
                FD_ZERO(&read_fd_set);

                shutdown = 1;
            }

            if(!strncmp(buff, "subscribe", 9)) {

                char topic_title[TOPIC_LEN + 1] = { 0 };
                int sfopt;
                sscanf(buff, "subscribe %s %d", topic_title, &sfopt);

                if (strlen(topic_title) == 0 || (sfopt != 0 && sfopt != 1)) {
                    ERR("skipping execution on bad input");
                    continue;
                }

                strncpy(buffered_pkt.tcp_pkt.id, argv[1], ID_LEN);
                strcpy(buffered_pkt.tcp_pkt.intent, buff);
                proto_send(sockfd, &buffered_pkt, sizeof(buffered_pkt.tcp_pkt));

                printf("Subscribed to topic.\n");
            }

            if(!strncmp(buff, "unsubscribe", 11)) {

                char topic_title[TOPIC_LEN + 1] = { 0 };
                sscanf(buff, "unsubscribe %s", topic_title);

                if (strlen(topic_title) == 0) {
                    ERR("skipping execution on bad input");
                    continue;
                }

                strncpy(buffered_pkt.tcp_pkt.id, argv[1], ID_LEN);
                strcpy(buffered_pkt.tcp_pkt.intent, buff);
                proto_send(sockfd, &buffered_pkt, sizeof(buffered_pkt.tcp_pkt));

                printf("Unsubscribed from topic.\n");
            }

            continue;
        }

        if (FD_ISSET(sockfd, &temp_fd_set)) {

            memset(&buffered_pkt, 0, sizeof(buffered_pkt));
            proto_recv(sockfd, &buffered_pkt, sizeof(buffered_pkt.notify_pkt));

            if (buffered_pkt.notify_pkt.type == EXIT) {
                shutdown = 1;
                continue;
            }

            interpret_notification(&buffered_pkt);

        }

    }

    free(buff);

    close(sockfd);

    return 0;
}
