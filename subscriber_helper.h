#ifndef SUBSCRIBER_HELPER_H
#define SUBSCRIBER_HELPER_H

#include "helper.h"

bool verify_subscribe(char payload[BUFLEN]) {
    char copy_payload[BUFLEN];
    char topic[BUFLEN];
    char SF[BUFLEN];
    char *p;

    strcpy(copy_payload, payload);
    
    // verify number of arguments
    int spaces = 0;
    p = strchr(copy_payload, ' ');
    while(p != NULL) {
        spaces++;
        p = strchr(p + 1, ' ');
    }
    if (spaces != 2) {
        return false;
    }

    // verify arguments
    p = strtok(copy_payload, " "); //subscribe
    p = strtok(NULL, " "); //topic
    strcpy(topic, p);

    p = strtok(NULL, " \n"); //SF
    strcpy(SF, p);

    if (strlen(SF) != 1 && (SF[0] != 0 && SF[0] != 1)) {
        return false;
    }

    return true;
}

bool verify_unsubscribe(char payload[BUFLEN]) {
    char copy_payload[BUFLEN];
    char topic[BUFLEN];
    char *p;

    strcpy(copy_payload, payload);

    // verify numerb of arguments
    int spaces = 0;
    p = strchr(copy_payload, ' ');
    while(p != NULL) {
        p = strchr(p + 1, ' ');
        spaces++;
    }
    if (spaces != 1) {
        return false;
    }

    // verify arguments
    p = strtok(copy_payload, " "); //subscribe
    p = strtok(NULL, " "); //topic
    strcpy(topic, p);

    if (topic == NULL) {
        return false;
    }

    return true;
}

#endif