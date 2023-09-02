#include "../include/VideoUDPShared.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int VideoUDPSharedCreateSocket(struct sockaddr_in* localAddress) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("Error: socket creation error");
        exit(EXIT_FAILURE);
    }
    int result = 1;
    result     = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &result, sizeof(int));
    if (result < 0) {
        perror("Error: set socket options error");
        exit(EXIT_FAILURE);
    }
    result = fcntl(fd, F_GETFL, 0);
    if (result < 0) {
        perror("Error: get socket flags error");
        exit(EXIT_FAILURE);
    }
    result &= ~O_NONBLOCK;
    result = fcntl(fd, F_SETFL, result);
    if (result < 0) {
        perror("Error: set socket flags error");
        exit(EXIT_FAILURE);
    }
    result = bind(fd, (struct sockaddr*)localAddress, sizeof(struct sockaddr_in));
    if (result < 0) {
        perror("Error: bind socket error");
        exit(EXIT_FAILURE);
    }
    return fd;
}