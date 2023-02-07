#ifndef HELPER_H
#define HELPER_H

#include <netinet/tcp.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string.h>
#include <queue>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <math.h>

//UDP type
#define INT 0
#define SHORT_REAL 1
#define FLOAT 2
#define STRING 3

#define BUFLEN 256 // max size data
#define MAX_CLIENTS	5 // max waiting clients

// UDP macros
#define UDP_TOPIC_LEN 50
#define UDP_CONTENT_LEN 1500

#define MAX_TCP_ID_LEN 10

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

/*
 * Macro verify errors
 * Exemple:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */
#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#endif