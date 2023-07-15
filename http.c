#define _POSIX_C_SOURCE 200112L
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
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

#define REQUEST_PATH "./request.http"

#define RESPONSE_CAP 16384

static char *mmap_whole_file(const char *path, size_t *out_len) {
    char *out = NULL;

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "ERROR: Could not open file %s: %s\n", path,
                strerror(errno));
        exit(1);
    }

    struct stat statbuf = {0};
    if (fstat(fd, &statbuf) < 0) {
        fprintf(stderr, "ERROR: Could not stat file %s: %s\n", path,
                strerror(errno));
        exit(1);
    }
    size_t len = statbuf.st_size;

    out = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
    if (out == MAP_FAILED) {
        fprintf(stderr, "ERROR: Could not map file %s: %s\n", path,
                strerror(errno));
        exit(1);
    }

    printf("Request:\n%*s\n", (int)len, out);

    if (close(fd) < 0) {
        fprintf(stderr, "ERROR: Could not close file %s: %s\n", path,
                strerror(errno));
        exit(1);
    }

    *out_len = len;
    return out;
}

static inline void usage(FILE *f, const char *program) {
    fprintf(f, "USAGE: %s <response-file-path>\n", program);
}

int main(int argc, char **argv) {
    (void)argc;
    const char *program = *argv++;
    assert(program);

    // TODO: Change hardcoded values
    const char *hostname = "www.google.com";

    const char *response_path = *argv++;
    if (!response_path) {
        usage(stderr, program);
        fprintf(stderr, "ERROR: No response path specified.\n");
        exit(1);
    }

    char *request = NULL;
    size_t request_len;
    request = mmap_whole_file(REQUEST_PATH, &request_len);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "ERROR: Could not open socket: %s\n", strerror(errno));
        exit(1);
    }

    struct addrinfo hint = {0};
    struct addrinfo *result = NULL;

    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_flags = AI_CANONNAME;

    if ((errno = getaddrinfo(hostname, "http", &hint, &result)) != 0) {
        fprintf(stderr, "ERROR: Could not get addrinfo for hostname %s: %s\n",
                hostname, gai_strerror(errno));
        exit(1);
    }

    display_addrinfo_all(result);

    printf("Connecting...\n");
    assert(result->ai_family == AF_INET);
    assert(result->ai_socktype == SOCK_STREAM);
    if (connect(sockfd, result->ai_addr, result->ai_addrlen) < 0) {
        fprintf(stderr, "ERROR: Failed to connect: %s\n", strerror(errno));
        exit(1);
    }
    printf("Connected.\n");

    {
        printf("Sending request...\n");
        size_t len = strlen(request);
        size_t sent = 0;
        do {
            ssize_t bytes = write(sockfd, &request[sent], len - sent);
            if (bytes < 0) {
                fprintf(stderr, "ERROR: Could not write to socket: %s\n",
                        strerror(errno));
                exit(1);
            }
            if (bytes == 0) {
                break;
            }
            sent += bytes;
            printf("Sent %zu bytes (%zu total so far).\n", bytes, sent);
        } while (sent < len);
    }

    {
        int fd = open(response_path, O_CREAT | O_RDWR);
        if (fd < 0) {
            int saved = errno;
            fprintf(stderr, "ERROR: Could not open file %s: %s\n",
                    response_path, strerror(saved));

            if (saved == EACCES) {
                fprintf(stderr, "This could mean you need to `chmod +rw %s`\n",
                        response_path);
            }

            exit(1);
        }

        printf("Awaiting response...\n");
        size_t received = 0;
        while (true) {
            char response[RESPONSE_CAP] = {0};
            ssize_t bytes = read(sockfd, response, RESPONSE_CAP - 1);
            if (bytes < 0) {
                fprintf(stderr, "ERROR: Could not read from socket: %s\n",
                        strerror(errno));
                exit(1);
            }
            if (bytes == 0) {
                break;
            }
            received += bytes;
            if (write(fd, response, bytes) < 0) {
                fprintf(stderr, "ERROR: Could not write to file %s: %s\n",
                        response_path, strerror(errno));
                exit(1);
            }
            printf("Received %zu bytes (%zu total so far).\n", bytes, received);
        }

        if (close(fd) < 0) {
            fprintf(stderr, "ERROR: Could not close file %s: %s\n",
                    response_path, strerror(errno));
            exit(1);
        }
    }

    freeaddrinfo(result);
    munmap(request, request_len);

    if (close(sockfd) < 0) {
        fprintf(stderr, "ERROR: Could not close socket: %s\n", strerror(errno));
        exit(1);
    }

    return 0;
}
