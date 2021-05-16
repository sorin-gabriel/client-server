#include "includes.h"

int main(int argc, char const *argv[]) {

    EXIT_IF(argc != 2, "wrong number of arguments");

    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    int retval, optval = 1;

    struct sockaddr_in srv;
    srv.sin_family = AF_INET;
    srv.sin_port = htons(atoi(argv[1]));;
    srv.sin_addr.s_addr = INADDR_ANY;

    struct sockaddr_in clnt;
    socklen_t clnt_size = sizeof(clnt);

    int sockfd_on_tcp = socket(AF_INET, SOCK_STREAM, 0);
    EXIT_IF(sockfd_on_tcp < 0, "dead socket");

    retval = bind(sockfd_on_tcp, (struct sockaddr *)&srv,
        sizeof(struct sockaddr));
    EXIT_IF(retval < 0, "dead socket");

    retval = listen(sockfd_on_tcp, SOMAXCONN);
    EXIT_IF(retval < 0, "dead socket");

    retval = setsockopt(sockfd_on_tcp, IPPROTO_TCP, TCP_NODELAY, &optval,
        sizeof(int));
    EXIT_IF(retval < 0, "dead socket");

    retval = setsockopt(sockfd_on_tcp, SOL_SOCKET, SO_REUSEADDR, &optval,
        sizeof(int));
    EXIT_IF(retval < 0, "dead socket");

    int sockfd_on_udp = socket(AF_INET, SOCK_DGRAM, 0);
    EXIT_IF(sockfd_on_udp < 0, "dead socket");

    retval = bind(sockfd_on_udp, (struct sockaddr *)&srv,
        sizeof(struct sockaddr));
    EXIT_IF(retval < 0, "dead socket");

    retval = setsockopt(sockfd_on_udp, SOL_SOCKET, SO_REUSEADDR, &optval,
        sizeof(int));
    EXIT_IF(retval < 0, "dead socket");

    fd_set read_fd_set, temp_fd_set;
    int maxfd;

    FD_ZERO(&read_fd_set);
    FD_SET(sockfd_on_tcp, &read_fd_set);
    FD_SET(sockfd_on_udp, &read_fd_set);
    FD_SET(STDIN_FILENO, &read_fd_set);
    
    maxfd =
        STDIN_FILENO >= sockfd_on_tcp &&
        STDIN_FILENO >= sockfd_on_udp ?
        STDIN_FILENO :
            sockfd_on_tcp >= sockfd_on_udp ?
            sockfd_on_tcp : sockfd_on_udp;

    proto_pkt buffered_pkt = { 0 };

    topics_table *topics = init_topics();
    clients_table *clients = init_clients();

    char *buff = (char *)malloc(PROTO_BUFF * sizeof(char));

    int shutdown = 0;
    while (!shutdown) {

        temp_fd_set = read_fd_set;
        retval = select(maxfd + 1, &temp_fd_set, NULL, NULL, NULL);
        EXIT_IF(retval < 0, "bad socket selection");

        for (int i = 0; i < maxfd + 1; i++) {        
    
            if (FD_ISSET(i, &temp_fd_set)) {

                memset(&buffered_pkt, 0, sizeof(buffered_pkt));

                if (i == sockfd_on_udp) {

                    recv_from_udp(i, &buffered_pkt);
                    topic_notify(topics, clients, &buffered_pkt);

                    continue;
                }

                if (i == sockfd_on_tcp) {

                    int sockfd_new_clnt = accept(sockfd_on_tcp,
                        (struct sockaddr *)&clnt, &clnt_size);
                    EXIT_IF(sockfd_new_clnt < 0, "dead socket");
                    
                    proto_recv(sockfd_new_clnt, &buffered_pkt,
                        sizeof(buffered_pkt.tcp_pkt));

                    char crt_clnt_id[ID_LEN + 1] = { 0 };
                    strncpy(crt_clnt_id, buffered_pkt.tcp_pkt.id, ID_LEN);

                    if (connect_client(clients, sockfd_new_clnt, crt_clnt_id) < 0) {

                        buffered_pkt.notify_pkt.type = EXIT;
                        proto_send(sockfd_new_clnt, &buffered_pkt,
                            sizeof(buffered_pkt.notify_pkt));

                        printf("Client %s already connected.\n", crt_clnt_id);

                        continue;
                    }

                    FD_SET(sockfd_new_clnt, &read_fd_set);
                    maxfd =
                        maxfd >= sockfd_new_clnt ?
                        maxfd : sockfd_new_clnt;

                    greet(crt_clnt_id, &clnt);

                    continue;
                }

                if (i == STDIN_FILENO) {

                    scanf("%s", buff);
                    if (!strncmp(buff, "exit", 4)) {

                        buffered_pkt.notify_pkt.type = EXIT;
                        for (int j = 0; j < clients->size; j++) {
                            if (clients->entries[i].status == ONLINE) {
                                proto_send(clients->entries[j].sockfd,
                                    &buffered_pkt, sizeof(buffered_pkt.notify_pkt));
                            }
                        }

                        close(sockfd_on_tcp);
                        close(sockfd_on_udp);

                        FD_ZERO(&read_fd_set);
                        FD_ZERO(&temp_fd_set);

                        shutdown = 1;
                    }

                    continue;
                }

                proto_recv(i, &buffered_pkt, sizeof(buffered_pkt.tcp_pkt));

                char crt_clnt_id[ID_LEN + 1] = { 0 };
                strncpy(crt_clnt_id, buffered_pkt.tcp_pkt.id, ID_LEN);

                if (!strncmp(buffered_pkt.tcp_pkt.intent, "exit", 4)) {

                    mark_offline_client(clients, buffered_pkt.tcp_pkt.id);
                    goodbye(crt_clnt_id);

                    close(i);
                    FD_CLR(i, &read_fd_set);

                    continue;
                }

                if (!strncmp(buffered_pkt.tcp_pkt.intent, "subscribe", 9)) {
                    char topic_title[TOPIC_LEN + 1] = { 0 };
                    int sfopt;
                    sscanf(buffered_pkt.tcp_pkt.intent, "subscribe %s %d",
                        topic_title, &sfopt);

                    if (NULL == findTopicWithTitle(topics, topic_title)) {
                        create_topic(topics, topic_title);
                    }

                    client *my_client = findClientWithId(clients, crt_clnt_id);
                    subscribe(topics, my_client, topic_title, sfopt);
                }
            
                if (!strncmp(buffered_pkt.tcp_pkt.intent, "unsubscribe", 11)) {
                    char topic_title[TOPIC_LEN + 1] = { 0 };
                    sscanf(buffered_pkt.tcp_pkt.intent, "unsubscribe %s",
                        topic_title);

                    client *my_client = findClientWithId(clients, crt_clnt_id);
                    unsubscribe(topics, my_client, topic_title);
                }

            }
        }

    }

    free(buff);

    return 0;
}
