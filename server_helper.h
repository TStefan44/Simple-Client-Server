#ifndef SERVER_HELPER_H
#define SERVER_HELPER_H

#include "helper.h"

typedef struct udp_message {
    char type;
    char topic[UDP_TOPIC_LEN];
    char content[UDP_CONTENT_LEN];
} udp_message;

typedef struct content {
    char cont[UDP_CONTENT_LEN + UDP_TOPIC_LEN + 100];
} content;

typedef struct topic {
    char topic_name[UDP_TOPIC_LEN];
    std::queue<content> to_send;
    bool SF;
} topic;

typedef struct subscribers {
    char id[MAX_TCP_ID_LEN];
    int socket;
    bool connected;
    std::vector<topic> topics;
} subscribers;

/**
 * @brief Search sub by their id
 * 
 * @param my_id id to find
 * @param subs list of subs
 * @return found match sub otherwise NULL
 */
subscribers* find_subscriber_by_id(char my_id[MAX_TCP_ID_LEN], std::vector<subscribers*> subs) {
    int size = subs.size();
    
    for (int i = 0; i < size; i++) {
        if (strcmp(my_id, subs[i]->id) == 0) {
            // Match found
            return subs[i];
        }
    }

    // No match
    return NULL;
}

/**
 * @brief Search sub by socket
 * 
 * @param socket to find
 * @param subs list of subs
 * @return found match sub otherwise NULL
 */
subscribers* find_subscriber_by_socket(int socket, std::vector<subscribers*> subs) {
    int size = subs.size();

    for (int i = 0; i < size; i++) {
        if (subs[i]->socket == socket) {
            return subs[i];
        }
    }

    return NULL;
}

/**
 * @brief New client connected to server
 * 
 * @param id client's id
 * @param socket client's socket
 * @return subscribers* new client saw as a subscribers
 */
subscribers* new_sub(char id[MAX_TCP_ID_LEN], int socket) {
    subscribers *new_sub = (subscribers*)malloc(1 * sizeof(subscribers));
    
    strcpy(new_sub->id, id);
    new_sub->socket = socket;
    new_sub->connected = true;

    return new_sub;
}

/**
 * @brief Send all messages saved in queue to the client
 * 
 * @param to_send queue of messages from current topic
 * @param socket socket destination
 */
void send_SF(std::queue<content> &to_send, int socket) {
    char buffer[UDP_CONTENT_LEN + UDP_TOPIC_LEN + 100];
    int n;

    if (to_send.empty()) {
        // Nothing to send
        return;
    }

    while (!to_send.empty()) {
        // extract content from queue
        memset(buffer, 0, UDP_CONTENT_LEN + UDP_TOPIC_LEN + 100);
        content cont = to_send.front();
        to_send.pop();

        // sent message to client
        strcpy(buffer, cont.cont);
        n = send(socket, buffer, strlen(cont.cont), 0);
        DIE(n < 0, "send topics with SF = 1");
    }
}

/**
 * @brief Send to client messages from subscribed topic with SF = 1
 * 
 * @param sub current client
 */
void send_messages_from_topic(subscribers *sub) {
    int topic_size = sub->topics.size();
    topic currtopic; 

    for (int i = 0; i < topic_size; i++) {
        currtopic = sub->topics[i];

        if (currtopic.SF == 0) {
            // no store-and-forward for this topic
            continue;
        } else {
            // store-and-forward for this topic
            send_SF(sub->topics[i].to_send, sub->socket);
        }
    }
    
}

void close_subs(std::vector<subscribers*> subs) {
    for (int i = 0; i < subs.size(); i++) {
        if (subs[i]->connected == true) {
            send(subs[i]->socket, "", 0, 0);
            // Client <ID_CLIENT> disconnected.
            printf("Client %s disconnected.\n", subs[i]->id);
            close(subs[i]->socket);
        }
    }
}

void extract_subscribe(char input[BUFLEN], char topic[UDP_TOPIC_LEN], bool &SF) {
    char *extract = strtok(input, " "); // subscribe word
    extract = strtok(NULL, " "); // topic
    strcpy(topic, extract);

    extract = strtok(NULL, " \n"); //SF
    SF = (extract[0] == '1')? true : false;
}

char* formatUDP_message(char payload[UDP_CONTENT_LEN + UDP_TOPIC_LEN + 100], char **saved_topic) {
    char *message = (char*)calloc(UDP_CONTENT_LEN + UDP_TOPIC_LEN + 100, sizeof(char));
    char topic[UDP_TOPIC_LEN];
    strcpy(topic, payload);

    *saved_topic = (char*)calloc(UDP_TOPIC_LEN, sizeof(char));
    strcpy(*saved_topic, topic);
    

    sprintf(message, "%s - ", topic);

    int type = (int) payload[50];
    switch(type) {
        case 0: {
            // extract byte sign
            int multiply = (payload[51] == 0)? 1 : -1;

            // extract the integer
            uint32_t data;
            memcpy(&data, payload + 52, 4);
            int info = htonl(data);
            info *= multiply;

            // put the integer in message
            sprintf(message + strlen(topic) + 1, "INT - %d", info);
            break;
        }
        case 1: {
            // extract number
            uint16_t data;
            memcpy(&data, payload + 51, 2);

            // convert to short, extract real and zecimal part
            uint16_t info = htons(data);
            uint16_t real = info / 100;
            uint16_t zec = info % 100;

            // put short_real in message
            if (zec < 10) {
                sprintf(message + strlen(topic) + 1, "SHORT_REAL - %d.0%d", real, zec);
            } else {
                sprintf(message + strlen(topic) + 1, "SHORT_REAL - %d.%d", real, zec);
            }
            break;
        }
        case 2: {
            // extract byte sign
            int multiply = (payload[51] == 0)? 1 : -1;

            // extract float
            uint32_t data;
            memcpy(&data, payload + 52, 4);
            int modulo = htonl(data);

            // extract pow
            int my_pow = pow(10, payload[56]);

            // construct float
            int real = multiply * modulo / my_pow;
            int zec = modulo % my_pow;

            // put float in message
            if (my_pow == 1) {
                sprintf(message + strlen(topic) + 1, "FLOAT - %d", real);
            } else {
                sprintf(message + strlen(topic) + 1, "FLOAT - %d.%d", real, zec);
            }
            break;
        }
        case 3: {
            char *array = (char*)calloc(UDP_CONTENT_LEN, sizeof(char));
            strcpy(array, payload + 51);
            sprintf(message + strlen(topic) + 1, "STRING - %s", array);
        }
    }

    return message;
}

#endif