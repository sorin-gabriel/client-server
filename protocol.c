#include "includes.h"

/*
    use buffered_pkt to recieve a packet from a udp client and
    add the udp client metadata to the packet 
*/
void recv_from_udp(int sockfd, proto_pkt *buffered_pkt) {
    struct sockaddr_in udp_clnt;
    socklen_t udp_clnt_size = sizeof(udp_clnt);

    int retval = recvfrom(sockfd, &buffered_pkt->udp_pkt, PROTO_BUFF, 0,
        (struct sockaddr *)&udp_clnt, &udp_clnt_size);
    EXIT_IF(retval < 0, "recv_from_udp failed");

    buffered_pkt->notify_pkt.udp_ip.s_addr = udp_clnt.sin_addr.s_addr;
    buffered_pkt->notify_pkt.udp_port = udp_clnt.sin_port;
}

void proto_recv(int sockfd, proto_pkt *pkt, size_t length) {

    size_t rx = 0;
    while (rx < length) {
        int retval = recv(sockfd, (char *)pkt + rx, length - rx, 0);
        EXIT_IF(retval < 0, "proto_recv failed");

        rx += retval;
    }
}

void proto_send(int sockfd, proto_pkt *pkt, size_t length) {

    size_t tx = 0;
    while (tx < length) {
        int retval = send(sockfd, (char *)pkt + tx, length - tx, 0);
        EXIT_IF(retval < 0, "proto_send failed");

        tx += retval;
    }
}

void greet(char *client_id, struct sockaddr_in *clnt) {
    printf("New client %s connected from %s:%d.\n", client_id,
        inet_ntoa(clnt->sin_addr), ntohs(clnt->sin_port));
}

void goodbye(char *client_id) {
    printf("Client %s disconnected.\n", client_id);
}

void subscribe(topics_table *table, client *crt_client, char *title, int sf) {
    for (int i = 0; i < table->size; i++) {

        if (!strncmp(title, table->entries[i].title, TOPIC_LEN)) {

            subscriptions_table *sub_tbl = &table->entries[i].subs;

            for (int j = 0; j < sub_tbl->size; j++) {

                subscription *sub = &sub_tbl->entries[j];

                if (!strncmp(crt_client->id, sub->id, ID_LEN)) {
                    sub->sf = sf;
                    return;
                }

            }

            if (sub_tbl->size == sub_tbl->capacity) {
                sub_tbl->capacity *= 2;
                sub_tbl->entries = realloc(sub_tbl->entries,
                    sub_tbl->capacity * sizeof(*(sub_tbl->entries)));
            }

            subscription *sub = &sub_tbl->entries[sub_tbl->size];

            strncpy(sub->id, crt_client->id, ID_LEN);            
            sub->sf = sf;

            sub_tbl->size++;
            return;
        }

    }
}

void unsubscribe(topics_table *table, client *crt_client, char *title) {
    for (int i = 0; i < table->size; i++) {

        if (!strncmp(title, table->entries[i].title, TOPIC_LEN)) {

            subscriptions_table *sub_tbl = &table->entries[i].subs;

            for (int j = 0; j < sub_tbl->size; j++) {

                subscription *sub = &sub_tbl->entries[j];
                
                if (!strncmp(crt_client->id, sub->id, ID_LEN)) {
                    if (j < sub_tbl->size - 1) {
                        memcpy(&sub, &sub_tbl->entries[sub_tbl->size - 1],
                            sizeof(subscription));
                    }
                    sub_tbl->size--;
                    return;
                }

            }
            return;
        }

    }
}

void send_unread(client *clnt) {
    notifications_table *notifs = &clnt->notifications;

    for (int i = 0; i < notifs->size; i++) {
        proto_send(clnt->sockfd, &notifs->entries[i],
            sizeof(notifs->entries[i].notify_pkt));
    }

    notifs->size = 0;
}

int connect_client(clients_table *table, int sockfd, char *id) {
    for (int i = 0; i < table->size; i++) {

        if (!strncmp(id, table->entries[i].id, ID_LEN)) {

            client *clnt = &table->entries[i];

            if (clnt->status == ONLINE) {
                return -1;
            }

            clnt->sockfd = sockfd;
            clnt->status = ONLINE;
            send_unread(clnt);
            return 0;
        }

    }

    connect_new_client(table, sockfd, id);
    return 0;
}

void connect_new_client(clients_table *table, int sockfd, char *id) {
    if (table->size == table->capacity) {
        table->capacity *= 2;
        table->entries = realloc(table->entries,
            table->capacity * sizeof(*(table->entries)));
    }

    client *clnt = &table->entries[table->size];

    strncpy(clnt->id, id, ID_LEN);
    clnt->sockfd = sockfd;
    clnt->status = ONLINE;
    
    notifications_table *notifs = &clnt->notifications;

    notifs->size = 0;
    notifs->capacity = 1;
    notifs->entries = calloc(notifs->capacity, sizeof(*(notifs->entries)));
    table->size++;
}

void mark_offline_client(clients_table *table, char *id) {
    for (int i = 0; i < table->size; i++) {

        if (!strncmp(id, table->entries[i].id, ID_LEN)) {
            table->entries[i].status = OFFLINE;
            break;
        }

    }
}

void topic_notify(topics_table *topics, clients_table *clients, proto_pkt *pkt) {
    for (int i = 0; i < topics->size; i++) {

        if (!strncmp(pkt->notify_pkt.topic, topics->entries[i].title, TOPIC_LEN)) {
            /*
                after we have found the topic in the table, iterate
                through the subscriptions; if a subscribed client is online,
                send the packet right away; if a subscribed client with SF
                set to 1 is offline, put the packet into a queue
            */

            subscriptions_table *sub_tbl = &topics->entries[i].subs;
            
            for (int j = 0; j < sub_tbl->size; j++) {

                subscription *sub = &sub_tbl->entries[j];
                client *clnt = findClientWithId(clients, sub->id);

                if (clnt->status == ONLINE) {
                    proto_send(clnt->sockfd, pkt, sizeof(pkt->notify_pkt));
                    continue;
                }

                if (clnt->status == OFFLINE && sub->sf != 0) {

                    notifications_table *notifs = &clnt->notifications;

                    if (notifs->size == notifs->capacity) {
                    notifs->capacity *= 2;
                    notifs->entries = realloc(notifs->entries,
                        notifs->capacity * sizeof(*(notifs->entries)));
                    }

                    memcpy(&notifs->entries[notifs->size], pkt,
                        sizeof(pkt->notify_pkt));
                    notifs->size++;

                    continue;
                }
            
            }
        }

    }
}

void create_topic(topics_table *table, char *title) {
    if (table->size == table->capacity) {
        table->capacity *= 2;
        table->entries = realloc(table->entries,
            table->capacity * sizeof(*(table->entries)));
    }

    topic *tp = &table->entries[table->size];

    strncpy(tp->title, title, TOPIC_LEN);
    tp->subs.size = 0;
    tp->subs.capacity = 1;
    tp->subs.entries = calloc(tp->subs.capacity, sizeof(*(tp->subs.entries)));
    table->size++;
}

client *findClientWithId(clients_table *table, char *id) {
    for (int i = 0; i < table->size; i++) {
        if (!strncmp(id, table->entries[i].id, ID_LEN)) {
            return &table->entries[i];
        }
    }
    return NULL;
}

topic *findTopicWithTitle(topics_table *table, char *title) {
    for (int i = 0; i < table->size; i++) {

        if (!strncmp(table->entries[i].title, title, TOPIC_LEN)) {
            return &table->entries[i];
        }

    }
    return NULL;
}

void interpret_notification(proto_pkt *pkt) {
    char topic[TOPIC_LEN + 1] = { 0 };
    strncpy(topic, pkt->notify_pkt.topic, TOPIC_LEN);

    char *udp_ip = inet_ntoa(pkt->notify_pkt.udp_ip);
    uint16_t udp_port = ntohs(pkt->notify_pkt.udp_port);

    char *payload = pkt->notify_pkt.content;

    int payload_int;
    double payload_real;
    double payload_float;
    int mantissa;
    int exponent;
    char payload_string[CONTENT_LEN + 1] = { 0 };

    switch (pkt->notify_pkt.type) {

        case INT:
            payload_int = ntohl(*(uint32_t *)(&payload[1]));
            if (payload[0] == 0) {                    
                printf("%s:%hu - %s - INT - %d\n", udp_ip, udp_port,
                    topic, payload_int);
            } else {
                printf("%s:%hu - %s - INT - -%d\n", udp_ip, udp_port,
                    topic, payload_int);
            }
            break;

        case SHORT_REAL:
            payload_real = ((double) ntohs(*(uint16_t *)payload)) / 100;
            printf("%s:%hu - %s - SHORT_REAL - %.2lf\n", udp_ip, udp_port,
                topic, payload_real);
            break;

        case FLOAT:
            mantissa = ntohl(*(uint32_t *)(&payload[1]));
            exponent = payload[1 + sizeof(uint32_t)];
            payload_float = 1.0 * ((double) mantissa / pow(10, exponent));
            if (payload[0] == 0) {
                printf("%s:%hu - %s - FLOAT - %lf\n", udp_ip, udp_port,
                    topic, payload_float);
            } else {
                printf("%s:%hu - %s - FLOAT - -%lf\n", udp_ip, udp_port,
                    topic, payload_float);
            }
            break;

        case STRING:
            strncpy(payload_string, pkt->notify_pkt.content, CONTENT_LEN);
            printf("%s:%hu - %s - STRING - %s\n", udp_ip, udp_port,
                topic, payload_string);
            break;
        
        default:
            ERR("bad data type or corrupt packet");
            break;
    }
}

clients_table *init_clients() {
    clients_table *table = malloc(sizeof(*table));
    table->size = 0;
    table->capacity = 1;
    table->entries = calloc(table->capacity, sizeof(*(table->entries)));
    return table;
}

topics_table *init_topics() {
    topics_table *table = malloc(sizeof(*table));
    table->size = 0;
    table->capacity = 1;
    table->entries = calloc(table->capacity, sizeof(*(table->entries)));
    return table;
}
