#define _POSIX_C_SOURCE 200112L
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"

void ping_icmp(int sockfd, const char *hostname) {
    struct addrinfo hint = {0};
    struct addrinfo *ai = NULL;

    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_RAW;
    hint.ai_flags = AI_CANONNAME;

    printf("Getting addrinfo for %s...\n", hostname);
    if ((errno = getaddrinfo(hostname, NULL, &hint, &ai)) != 0) {
        fprintf(stderr, "ERROR: Could not get addrinfo for hostname %s: %s\n",
                hostname, gai_strerror(errno));
        exit(1);
    }
    display_addrinfo_all(ai);

    struct icmp_pack msg = make_icmp_echo_pack();
    dump_bytes(stdout, &msg, sizeof(msg));

    printf("Sending echo packet...\n");
    if (sendto(sockfd, &msg, sizeof(msg), 0, ai->ai_addr, ai->ai_addrlen) < 0) {
        fprintf(stderr, "ERROR: Could not send echo packet to %s: %s\n",
                hostname, strerror(errno));
        exit(1);
    }
    printf("Sent successfully.\n");

    printf("Reading response...\n");
    if (recvfrom(sockfd, &msg, sizeof(msg), 0, ai->ai_addr, &ai->ai_addrlen) <
        0) {
        fprintf(stderr, "ERROR: Could not receive response from %s: %s\n",
                hostname, strerror(errno));
        exit(1);
    }
    printf("Received response with type %d.\n", msg.hdr.type);
    dump_bytes(stdout, &msg, sizeof(msg));

    freeaddrinfo(ai);
}

int main(void) {
    const char *hostname = "www.google.com";

    printf("Creating socket...\n");
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        fprintf(stderr, "ERROR: Could not open socket: %s\n", strerror(errno));
        exit(1);
    }
    printf("Created socket successfully.\n");

    ping_icmp(sockfd, hostname);

    printf("Closing socket...\n");
    close(sockfd);
    printf("Closed socket successfully.\n");
    return 0;
}
