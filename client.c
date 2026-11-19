/*
 * Local Network Chat Client - Project 39
 * Build: gcc -Wall -Wextra -std=c11 client.c -o client
 * Run:   ./client 127.0.0.1 5000 Ayush
 */
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>

#define BUFFER_SIZE 4096

static int send_all(int fd, const char *data, size_t length) {
    size_t sent = 0;
    while (sent < length) {
        ssize_t n = send(fd, data + sent, length - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <server-ip> <port> <nickname>\n", argv[0]);
        return 1;
    }

    char *end = NULL;
    long port = strtol(argv[2], &end, 10);
    if (!end || *end || port < 1 || port > 65535 ||
        strlen(argv[3]) == 0 || strlen(argv[3]) > 31 ||
        strchr(argv[3], ' ') || strchr(argv[3], '/')) {
        fprintf(stderr, "Invalid port or nickname (nickname: 1-31 chars, no spaces or '/').\n");
        return 1;
    }

    struct addrinfo hints, *result = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_text[16];
    snprintf(port_text, sizeof(port_text), "%ld", port);
    int gai = getaddrinfo(argv[1], port_text, &hints, &result);
    if (gai != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai));
        return 1;
    }

    int sock = -1;
    for (struct addrinfo *p = result; p; p = p->ai_next) {
        sock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sock < 0) continue;
        if (connect(sock, p->ai_addr, p->ai_addrlen) == 0) break;
        close(sock);
        sock = -1;
    }
    freeaddrinfo(result);

    if (sock < 0) {
        perror("Could not connect");
        return 1;
    }

    char nick_command[64];
    snprintf(nick_command, sizeof(nick_command), "/nick %s\n", argv[3]);
    if (send_all(sock, nick_command, strlen(nick_command)) < 0) {
        perror("send"); close(sock); return 1;
    }

    printf("Connected to %s:%ld as %s.\n", argv[1], port, argv[3]);
    printf("Commands: /nick name, /msg username message, /list, /quit\n");

    int running = 1;
    while (running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        int max_fd = sock > STDIN_FILENO ? sock : STDIN_FILENO;

        int ready = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("select"); break;
        }

        if (FD_ISSET(sock, &read_fds)) {
            char buffer[BUFFER_SIZE + 1];
            ssize_t n = recv(sock, buffer, BUFFER_SIZE, 0);
            if (n <= 0) {
                printf("\nDisconnected from server.\n");
                break;
            }
            buffer[n] = '\0';
            fputs(buffer, stdout);
            fflush(stdout);
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            char line[BUFFER_SIZE];
            if (!fgets(line, sizeof(line), stdin)) {
                running = 0;
                break;
            }
            size_t len = strlen(line);
            if (len == 0) continue;
            if (line[len - 1] != '\n') {
                int ch;
                while ((ch = getchar()) != '\n' && ch != EOF) {}
                fprintf(stderr, "Message too long; maximum is %d characters.\n",
                        BUFFER_SIZE - 2);
                continue;
            }
            if (send_all(sock, line, len) < 0) {
                perror("send");
                break;
            }
            if (strcmp(line, "/quit\n") == 0) {
                running = 0;
            }
        }
    }

    close(sock);
    return 0;
}
