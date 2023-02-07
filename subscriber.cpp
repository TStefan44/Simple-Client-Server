#include "subscriber_helper.h"

int main(int argc, char *argv[]) {
    
    if (argc < 4) {
        // no needed param
        usage(argv[0]);
    }

    // dezactivate buffering
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    // extract id, ip_server, port_server
    char* id = argv[1];
    char* ip_server = argv[2];
    char* port_server = argv[3];

    // setup descriptors
    fd_set read_fds, tmp_fds;
	int fdmax;
    int n;
    int running = 1;

    FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

    // socket preset
    struct sockaddr_in serv_addr;
    memset((char *) &serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(port_server));
    n = inet_aton(ip_server, &serv_addr.sin_addr);
    DIE(n == 0, "inet_aton");

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(socket < 0, "socket");
    fdmax = sockfd;

    n = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
    DIE(n < 0, "connect");

    // connect to server
    char payload[BUFLEN];
    memset(payload, 0, BUFLEN);
    strcpy(payload, id);
    n = send(sockfd, payload, strlen(payload), 0);
    DIE(n < 0, "connect server");

    int val = 1;
    n = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&val, sizeof(int));
    DIE(n < 0, "Nagle algorithm -> new socket");

    while (running == 1) {
        FD_SET(STDIN_FILENO, &read_fds);
	    FD_SET(sockfd, &read_fds);
        tmp_fds = read_fds;

        n = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
        DIE(n < 0, "select");

        memset(payload, 0, BUFLEN);
        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &tmp_fds)) {
                // input from keyboard

                if (i == STDIN_FILENO) {
                    fgets(payload, BUFLEN, stdin);

                    if (strncmp(payload, "subscribe", 9) == 0) {
                        // Subscribe
                        if (verify_subscribe(payload)) {
                            n = send(sockfd, payload, strlen(payload), 0);
                            DIE(n < 0, "Client subscribe");
                        }
                    } else if (strncmp(payload, "unsubscribe", 11) == 0) {
                        // Unsubscribe
                        if (verify_unsubscribe(payload)) {
                            n = send(sockfd, payload, strlen(payload), 0);
                            DIE(n < 0, "Client unsubscribe");
                        }
                    } else if (strncmp(payload, "exit", 4) == 0) {
                        n = send(sockfd, payload, strlen(payload), 0);
                        DIE (n < 0, "Exit client");
                        close(i);
                        running = 0;
                    }
                } else if(i == sockfd){
                    // data from server
                    int val = 1;
                    int neeg = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char*)&val, sizeof(int));
                    DIE(neeg < 0, "Nagle alg");

                    char received_data[UDP_CONTENT_LEN + UDP_TOPIC_LEN + 100];
                    memset(received_data, 0, UDP_CONTENT_LEN + UDP_TOPIC_LEN + 100);

                    n = recv(sockfd, received_data, sizeof(received_data), 0);
                    DIE(n < 0, "Server recv");

                    if (strlen(received_data) == 0) {
                        close(sockfd);
                        printf("Server has been closed!\n");
                        running = 0;
                    } else {
                        printf("%s\n", received_data);
                    }
                }
            }
        }
    }


    close(sockfd);
    return 0;
}

