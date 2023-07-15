#define _DEFAULT_SOURCE
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <limits.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"

// TODO: Rename
struct msg {
    struct iphdr iphdr;
    struct icmp_pack payload;
};

static inline void print_sum(FILE *f, const void *ptr, size_t bytes) {
    assert(bytes % 2 == 0);
    bytes /= 2;
    const uint16_t *shorts = ptr;

    uint16_t sum = 0;

    for (size_t i = 0; i < bytes; i++) {
        sum += shorts[i];
    }

    fprintf(f, "%d\n", sum);
}

// VERY UNFINISHED
// Still has a bunch of unnecessary debug printing
void ping_ip(int sockfd, const char *hostname) {
    struct addrinfo *ai_serv = NULL;

    // Get hostname's IP address
    {
        struct addrinfo hint = {0};

        hint.ai_family = AF_INET;
        hint.ai_socktype = SOCK_RAW;
        hint.ai_flags = AI_CANONNAME;

        printf("Getting addrinfo for %s...\n", hostname);
        if ((errno = getaddrinfo(hostname, NULL, &hint, &ai_serv)) != 0) {
            fprintf(stderr,
                    "ERROR: Could not get addrinfo for hostname %s: %s\n",
                    hostname, gai_strerror(errno));
            exit(1);
        }
        display_addrinfo_all(ai_serv);
    }

    struct sockaddr_in *addr_serv = (struct sockaddr_in *)ai_serv->ai_addr;
    struct sockaddr_in *addr_self = NULL;

    // Get own IP address
    {
        printf("Getting own IP address...\n");
        struct ifaddrs *ifap;
        if (getifaddrs(&ifap) < 0) {
            fprintf(stderr, "ERROR: Could not get own IP address: %s\n",
                    strerror(errno));
            exit(1);
        }

        for (struct ifaddrs *ifa = ifap; ifa; ifa = ifa->ifa_next) {
            int family = ifa->ifa_addr->sa_family;

            // Exclude 127.x.x.x
            if (ifa->ifa_flags & IFF_LOOPBACK) {
                continue;
            }

            if (family == AF_INET) {
                addr_self = (struct sockaddr_in *)ifa->ifa_addr;
            }
        }

        char buf[256];
        inet_ntop(AF_INET, &addr_self->sin_addr, buf, sizeof(buf));
        printf("Own IP address is %s.\n", buf);

        freeifaddrs(ifap);
    }

    struct icmp_pack payload = make_icmp_echo_pack();
    struct iphdr ip_hdr = {0};
    // TODO: Remove
    dump_bytes(stdout, &ip_hdr, sizeof(ip_hdr));
    ip_hdr.ihl = sizeof(struct iphdr) / sizeof(uint32_t);
    ip_hdr.version = 4;
    // RFC 2474 changed the meaning of this field. Unfortunately, I'm not smart
    // or patient enough to understand the whole specification document, so I
    // have no idea what this field actually does now. With that in mind, I'm
    // just going to set this to zero and hope for the best.
    ip_hdr.tos = 0;
    ip_hdr.tot_len = htons(sizeof(ip_hdr) + sizeof(payload));
    ip_hdr.id = htons(getpid());
    // Fragment offset
    // (first and only fragment)
    ip_hdr.frag_off = 0;
    ip_hdr.ttl = 64;
    ip_hdr.protocol = IPPROTO_ICMP;

    ip_hdr.saddr = addr_self->sin_addr.s_addr;
    ip_hdr.daddr = addr_serv->sin_addr.s_addr;

    // TODO: Remove
    printf("\n\n");
    dump_bytes(stdout, &ip_hdr, sizeof(ip_hdr));

    ip_hdr.check = checksum(&ip_hdr, sizeof(ip_hdr));

    // TODO: Remove
    printf("\n\n");
    dump_bytes(stdout, &ip_hdr, sizeof(ip_hdr));
    // TODO: Remove
    print_sum(stdout, &ip_hdr, sizeof(ip_hdr));

    struct msg msg = {
        .iphdr = ip_hdr,
        .payload = payload,
    };

    // TODO: Remove
    printf("\n\n");

    dump_bytes(stdout, &msg, sizeof(msg));

    printf("Sending echo packet...\n");
    if (sendto(sockfd, &msg, sizeof(msg), 0, ai_serv->ai_addr,
               ai_serv->ai_addrlen) < 0) {
        fprintf(stderr, "ERROR: Could not send echo packet to %s: %s\n",
                hostname, strerror(errno));
        exit(1);
    }
    printf("Sent successfully.\n");

    printf("Reading response...\n");
    if (recvfrom(sockfd, &msg, sizeof(msg), 0, ai_serv->ai_addr,
                 &ai_serv->ai_addrlen) < 0) {
        fprintf(stderr, "ERROR: Could not receive response from %s: %s\n",
                hostname, strerror(errno));
        exit(1);
    }

    freeaddrinfo(ai_serv);
    assert(0 && "Unfinished");
}

int main(void) {
    const char *hostname = "www.google.com";

    printf("Creating socket...\n");
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (sockfd < 0) {
        fprintf(stderr, "ERROR: Could not open socket: %s\n", strerror(errno));
        exit(1);
    }
    printf("Created socket successfully.\n");

    ping_ip(sockfd, hostname);

    printf("Closing socket...\n");
    close(sockfd);
    printf("Closed socket successfully.\n");
    return 0;
}
