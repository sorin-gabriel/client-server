#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#include <arpa/inet.h>

#include <sys/socket.h>
#include <sys/select.h>

#include <math.h>

#define ID_LEN 10
#define TOPIC_LEN 50
#define CONTENT_LEN 1500
#define INTENT_LEN 100
#define PROTO_BUFF 2048

#define EXIT_IF(cond, desc)                                             \
    do {                                                                \
        if (cond) {                                                     \
            fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);          \
            perror(desc);                                               \
            exit(EXIT_FAILURE);                                         \
        }                                                               \
    } while(0)

#define ERR(desc)                                                       \
    fprintf(stderr, "(%s, %d): ", __FILE__, __LINE__);                  \
    perror(desc)

typedef union {
    struct {
        char topic[TOPIC_LEN];
        unsigned char type;                 // one type from "content_type"
        char content[CONTENT_LEN];
    } udp_pkt;                              // udp_client to server

    struct {
        char topic[TOPIC_LEN];
        unsigned char type;                 // one type from "content_type"
        char content[CONTENT_LEN];
        struct in_addr udp_ip;
        in_port_t udp_port;
    } notify_pkt;                           // server to tcp_client

    struct {
        char id[ID_LEN];
        char intent[INTENT_LEN];
    } tcp_pkt;                              // tcp_client to server
} __attribute__((packed)) proto_pkt;

typedef enum {
    INT = 0,
    SHORT_REAL = 1,
    FLOAT = 2,
    STRING = 3,

    EXIT = 255
} content_type;

typedef enum {
    OFFLINE = 0,
    ONLINE = 1
} status;

typedef struct {
    int size;
    int capacity;
    proto_pkt *entries;
} notifications_table;

typedef struct {
    int sockfd;
    char id[ID_LEN + 1];
    status status;
    notifications_table notifications;      // used for topics with SF option
} client;

typedef struct {
    int size;
    int capacity;
    client *entries;
} clients_table;

typedef struct {
    char id[ID_LEN + 1];                    // contains the client id
    int sf;                                 // and SF option
} subscription;

typedef struct {
    int size;
    int capacity;
    subscription *entries;
} subscriptions_table;

typedef struct {
    char title[TOPIC_LEN + 1];              // a topic keeps track of subscribed
    subscriptions_table subs;               // users through subscriptions_table
} topic;

typedef struct {
    int size;
    int capacity;
    topic *entries;
} topics_table;

void recv_from_udp(int sockfd, proto_pkt *buffered_pkt);
void proto_recv(int sockfd, proto_pkt *pkt, size_t length);
void proto_send(int sockfd, proto_pkt *pkt, size_t length);

void greet(char *client_id, struct sockaddr_in *client_address);
void goodbye(char *client_id);

void subscribe(topics_table *table, client *clnt, char *topic_title, int sf);
void unsubscribe(topics_table *table, client *clnt, char *topic_title);

void send_unread(client *client);

int connect_client(clients_table *table, int sockfd, char *id);
void connect_new_client(clients_table *table, int sockfd, char *id);
void mark_offline_client(clients_table *table, char *id);

void topic_notify(topics_table *topics, clients_table *clients, proto_pkt *pkt);

void create_topic(topics_table *table, char *title);

client *findClientWithId(clients_table *table, char *id);
topic *findTopicWithTitle(topics_table *table, char *title);

void interpret_notification(proto_pkt *pkt);

clients_table *init_clients();
topics_table *init_topics();
