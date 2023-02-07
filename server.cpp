#include "server_helper.h"

int main(int argc, char *argv[])
{
    int running = 1;
    int n, ret;
    int newsockfd;
    struct sockaddr_in cli_addr;
    socklen_t clilen;

    std::vector<subscribers*> subs;

    if (argc < 1) {
        // no needed port
        usage(argv[0]);
    }

    // dezactivate buffering
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    //extract port number
    int portno;
    portno = atoi(argv[1]);
    DIE(portno == 0, "atoi"); //wrong port number

    // setup descriptors
    fd_set read_fds, tmp_fds;
	int fdmax;
    FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

    // bind preset
    struct sockaddr_in serv_addr;
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

    // UDP socket: set + bind + listen
    int udp_sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    DIE(udp_sockfd < 0, "UDP socket");
    n = bind(udp_sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
    DIE(n < 0, "UDP bind");
    FD_SET(udp_sockfd, &read_fds);

    // TCP socket: set + bind + listen
    int tcp_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(udp_sockfd < 0, "TCP socket");
    n = bind(tcp_sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
    DIE(n < 0, "TCP bind");
    n = listen(tcp_sockfd, MAX_CLIENTS);
    DIE(n < 0, "TCP listen");
    FD_SET(tcp_sockfd, &read_fds);

    // Nagle algorithm
    int val = 1;
    n = setsockopt(tcp_sockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&val, sizeof(int));
    DIE(n < 0, "Nagle algorithm -> TCP");

    FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(tcp_sockfd, &read_fds);
    FD_SET(udp_sockfd, &read_fds);

    fdmax = std::max(tcp_sockfd, udp_sockfd);

    while (running) {

        tmp_fds = read_fds;
        fdmax = std::max(tcp_sockfd, udp_sockfd);

        for (int i = 0; i < subs.size(); i++) {
            if(subs[i]->connected) {
                 fdmax = std::max(fdmax, subs[i]->socket);
            }
        }

        ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(ret < 0, "select");

        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &tmp_fds)) {
                //TCP client
                if (i == tcp_sockfd) {
                    // connect request on inactive socket, accept connection
                    clilen = sizeof(cli_addr);
                    newsockfd = accept(tcp_sockfd, (struct sockaddr *) &cli_addr, &clilen);
                    DIE(newsockfd < 0, "TCP accept");

                    n = setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&val, sizeof(int));
                    DIE(n < 0, "Nagle algorithm -> new socket");

                    // Receive and extract client ID
                    char id[MAX_TCP_ID_LEN];
                    memset(id, 0, MAX_TCP_ID_LEN);
                    n = recv(newsockfd, id, MAX_TCP_ID_LEN, 0);
                    DIE(n < 0, "ID client TCP");

                    // add new socket
                    FD_SET(newsockfd, &read_fds);
                    fdmax = std::max(fdmax, newsockfd);

                    //search subs by id
                    subscribers *sub = find_subscriber_by_id(id, subs);
                    if (sub == NULL) {
                        // new client
                        sub = new_sub(id, newsockfd);
                        subs.push_back(sub);

                        // New client <ID_CLIENT> connected from IP:PORT.
                        printf("New client %s connected from %s:%d.\n",
                            sub->id, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
                    } else {
                        // known id
                        if (sub->connected == true) {
                            // Client <ID_CLIENT> already connected.
                            printf("Client %s already connected.\n", sub->id);

                            // Close client
                            close(newsockfd);
                        } else {
                            // client was disconnected - reconnect
                            sub->connected = true;
                            sub->socket = newsockfd;

                            // save-and-forward
                            send_messages_from_topic(sub);

                            // New client <ID_CLIENT> connected from IP:PORT.
                            printf("New client %s connected from %s:%d.\n",
                                sub->id, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
                        }
                    }
                } else if (i == udp_sockfd) {
                    // UDP client
                    struct sockaddr_in from;
                    memset(&from, 0, sizeof(from));
                    socklen_t addrlen = sizeof(from);
                    
                    char payload[UDP_CONTENT_LEN + UDP_TOPIC_LEN + 100];
                    memset(payload, 0, UDP_CONTENT_LEN + UDP_TOPIC_LEN + 100);
                    n = recvfrom(udp_sockfd, payload, sizeof(payload), 0, (struct sockaddr *)&from, &addrlen);
                    DIE(n < 0, "UDP receve");

                    //extract data from udp message
                    char *udp_topic;
                    char *data = formatUDP_message(payload, &udp_topic);
                    char send_message[UDP_CONTENT_LEN + UDP_TOPIC_LEN + 100];
                    memset(send_message, 0, UDP_CONTENT_LEN + UDP_TOPIC_LEN + 100);
                    sprintf(send_message, "%s:%d - %s", inet_ntoa(from.sin_addr), ntohs(from.sin_port), data);

                    // send content
                    for (int k = 0; k < subs.size(); k++) {
                        for (int j = 0; j < subs[k]->topics.size(); j++) {
                            // match topic
                            if (strncmp(udp_topic, subs[k]->topics[j].topic_name, UDP_TOPIC_LEN) == 0) {
                                if (subs[k]->connected == true) {
                                    // conected client
                                    n = send(subs[k]->socket, send_message, strlen(send_message), 0);
                                    DIE(n < 0, "send UDP message to client");
                                } else if (subs[k]->topics[j].SF == 1) {
                                    // not connected but save-and-forward
                                    content my_content;
                                    strcpy(my_content.cont, send_message);
                                    subs[k]->topics[j].to_send.push(my_content);
                                }
                            }
                        }
                    }
                    
                } else if (i == STDIN_FILENO) {
                    char read_stdin[BUFLEN];
                    fgets(read_stdin, BUFLEN - 1, stdin);

                    if (strncmp(read_stdin, "exit", 4) == 0) {
                        // exit command
                        close_subs(subs);
                        FD_ZERO(&read_fds);
                        running = 0;
                    }
                } else {
                    // Commands from TCP clients
                    char input[BUFLEN];
                    memset(input, 0, BUFLEN);

                    n = setsockopt(newsockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&val, sizeof(int));
                    DIE(n < 0, "Nagle algorithm");

                    n = recv(i, input, sizeof(input), 0);
                    DIE(n < 0, "recv TCP");

                    // find client with current socket
                    subscribers *sub = find_subscriber_by_socket(i, subs);

                    if (sub != NULL) {
                        // Subscribe case

                        if (strncmp(input, "subscribe", 9) == 0) {
                            int skip = 0;
                            char my_topic[UDP_TOPIC_LEN];
                            bool SF;
                            extract_subscribe(input, my_topic, SF);

                            // verify is sub already subscribed
                            for (int j = 0; j < sub->topics.size(); j++) {
                                if (strcmp(my_topic, sub->topics[j].topic_name) == 0) {
                                    skip = 1;
                                    break;
                                }
                            }
                            if (skip == 1) {
                                continue;
                            }

                            // create new topic
                            topic new_topic;
                            strcpy(new_topic.topic_name, my_topic);
                            new_topic.SF = SF;

                            // add new topic
                            sub->topics.push_back(new_topic);

                            // notify client
                            char notify[] = "Subscribed to topic.";
                            n = send(i, notify, strlen(notify), 0);
                            DIE(n < 0, "server send subscribe notify");

                        } else if (strncmp(input, "unsubscribe", 11) == 0) {
                            // Unsubscribe case
                            // extract topic
                            char topic[UDP_TOPIC_LEN];
                            char *extract = strtok(input, " "); // unsubscribe
                            extract = strtok(NULL, " \n"); // topic
                            strcpy(topic, extract);

                            for (int j = 0; j < sub->topics.size(); j++) {
                                if (strcmp(topic, sub->topics[j].topic_name) == 0) {
                                    // remove topic
                                    sub->topics.erase(sub->topics.begin() + j);

                                    // notify client
                                    char notify[] = "Unsubscribed from topic.";
                                    n = send(i, notify, strlen(notify), 0);
                                    DIE(n < 0, "server send unsubscribe notify");
                                }
                            }
                        } else if (strncmp(input, "exit", 4) == 0) {
                            // close server case
                            sub->connected = false;
                            printf("Client %s disconnected.\n", sub->id);
                            close(i);
                            FD_CLR(i, &read_fds);
                        }
                    }
                }
            }
        }

    }

    // close sockets
    close(udp_sockfd);
    close(tcp_sockfd);

    return 0;    
}
