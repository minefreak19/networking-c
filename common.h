#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#define CRLF "\r\n"

// Probably this only needs to be 6. But memory is cheap.
#define PORT_STRING_LEN 16

#define ICMP_PACK_SZ 64

struct icmp_pack {
    struct icmphdr hdr;
    char data[ICMP_PACK_SZ - sizeof(struct icmphdr)];
};

static inline const char *socktype_to_string(int socktype) {
    switch (socktype) {
        case SOCK_STREAM:
            return "TCP";
        case SOCK_DGRAM:
            return "UDP";
        case SOCK_RAW:
            return "Raw";

        default:
            return "Unknown";
    }
}

static inline const char *family_to_string(int family) {
    switch (family) {
        case AF_INET:
            return "IPv4";
        case AF_INET6:
            return "IPv6";
        case AF_PACKET:
            return "Raw Packet";

        default:
            return "Unknown";
    }
}

static inline void display_addrinfo_all(struct addrinfo *ai) {
    while (ai) {
        char addr_string[INET6_ADDRSTRLEN];
        char port_string[PORT_STRING_LEN];

        void *addr;
        switch (ai->ai_family) {
            // IPv4
            case AF_INET: {
                struct sockaddr_in *sin = (struct sockaddr_in *)ai->ai_addr;
                addr = &sin->sin_addr;
                snprintf(port_string, PORT_STRING_LEN, "%d", sin->sin_port);
            } break;

            // IPv6
            case AF_INET6: {
                struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)ai->ai_addr;
                addr = &sin6->sin6_addr;
                snprintf(port_string, PORT_STRING_LEN, "%d", sin6->sin6_port);
            } break;

            default: {
                fprintf(stderr,
                        "ERROR: Invalid ai_family value %d specified.\n",
                        ai->ai_family);
                exit(1);
            }
        }
        inet_ntop(ai->ai_family, addr, addr_string, sizeof(addr_string));

        printf("Addrinfo {\n");
        printf("\tType: %s\n", socktype_to_string(ai->ai_socktype));
        if (ai->ai_canonname) {
            printf("\tName: %s\n", ai->ai_canonname);
        }
        printf("\tFamily: %s\n", family_to_string(ai->ai_family));
        printf("\tAddress: %s\n", addr_string);
        printf("\tPort: %s\n", port_string);
        printf("}\n");

        ai = ai->ai_next;
    }
}

static inline char hex_from_4_bits(uint8_t lower) {
    assert(lower <= 15);
    if (lower < 10) {
        return '0' + lower;
    }

    return 'A' + lower - 10;
}

static inline void dump_byte(FILE *f, uint8_t byte) {
    uint8_t upper = (byte & 0xF0) >> 4;
    uint8_t lower = (byte & 0x0F);

    fputc(hex_from_4_bits(upper), f);
    fputc(hex_from_4_bits(lower), f);
}

static inline void dump_bytes(FILE *f, const void *bytes, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (i && i % 4 == 0) fputc('\n', f);
        uint8_t x = *(((const uint8_t *)bytes) + i);
        dump_byte(f, x);
        fputc(' ', f);
    }
    fputc('\n', f);
}

static inline unsigned short checksum(void *b, int len) {
    unsigned short *buf = (unsigned short *)b;
    unsigned int sum = 0;
    unsigned short result;

    for (sum = 0; len > 1; len -= 2) sum += *buf++;
    if (len == 1) sum += *(unsigned char *)buf;
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);
    result = ~sum;
    return result;
}

static inline struct icmp_pack make_icmp_echo_pack(void) {
    printf("Preparing ICMP Echo packet of size %zu...\n",
           sizeof(struct icmp_pack));
    struct icmp_pack ret = {0};
    ret.hdr.type = ICMP_ECHO;
    ret.hdr.un.echo.id = htons(getpid());
    ret.hdr.un.echo.sequence = htons(0xCAFE);
    for (size_t i = 0; i < sizeof(ret.data); i++) {
        ret.data[i] = 32 + i;
    }
    ret.hdr.checksum = checksum(&ret, sizeof(ret));
    return ret;
}
