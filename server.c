/*
 * Local Network Chat Server - Project 39
 * POSIX/BSD TCP sockets; supports multiple clients with select().
 *
 * Build: gcc -Wall -Wextra -std=c11 server.c -o server
 * Run:   ./server 5000
 */
#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>

#define MAX_LINE 4096
#define LOG_FILE "chat_history.log"

typedef struct {
    int fd;
    char name[32];
    char input[MAX_LINE];
    size_t used;
} Client;

static Client *clients = NULL;
static size_t client_count = 0;

static int send_all(int fd, const char *data, size_t length) {
    size_t sent = 0;
    while (sent < length) {
        ssize_t n = send(fd, data + sent, length - sent, MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static void send_text(int fd, const char *text) {
    if (send_all(fd, text, strlen(text)) == 0)
        send_all(fd, "\n", 1);
}

static void log_message(const char *message) {
    FILE *fp = fopen(LOG_FILE, "a");
    if (!fp) {
        perror("Could not open chat log");
        return;
    }
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char stamp[32] = "unknown-time";
    if (tm_info) strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(fp, "[%s] %s\n", stamp, message);
    fclose(fp);
}

static void broadcast(const char *message, int except_fd) {
    char line[MAX_LINE + 128];
    snprintf(line, sizeof(line), "%s\n", message);
    for (size_t i = 0; i < client_count; ++i) {
        if (clients[i].fd != except_fd)
            send_all(clients[i].fd, line, strlen(line));
    }
}

static void remove_client(size_t index, const char *reason) {
    char notice[128];
    snprintf(notice, sizeof(notice), "*** %s %s", clients[index].name, reason);
    printf("%s\n", notice);
    log_message(notice);
    broadcast(notice, clients[index].fd);
    close(clients[index].fd);
    if (index + 1 < client_count)
        memmove(&clients[index], &clients[index + 1],
                (client_count - index - 1) * sizeof(Client));
    --client_count;
}

static int valid_name(const char *name) {
    size_t n = strlen(name);
    if (n == 0 || n >= sizeof(clients[0].name)) return 0;
    for (size_t i = 0; i < n; ++i) {
        if (name[i] == ' ' || name[i] == '\t' || name[i] == '/' ||
            name[i] == '\r' || name[i] == '\n')
            return 0;
    }
    return 1;
}

static int name_in_use(const char *name, int except_fd) {
    for (size_t i = 0; i < client_count; ++i)
        if (clients[i].fd != except_fd &&
            strcmp(clients[i].name, name) == 0)
            return 1;
    return 0;
}

static void list_users(int fd) {
    char result[MAX_LINE] = "Online users:";
    for (size_t i = 0; i < client_count; ++i) {
        char item[48];
        snprintf(item, sizeof(item), " %s", clients[i].name);
        if (strlen(result) + strlen(item) + 1 < sizeof(result))
            strcat(result, item);
    }
    send_text(fd, result);
}

static int handle_line(size_t index, char *line) {
    Client *c = &clients[index];
    while (*line == ' ' || *line == '\t') ++line;
    if (*line == '\0') return 0;

    if (strncmp(line, "/nick ", 6) == 0) {
        char *new_name = line + 6;
        while (*new_name == ' ') ++new_name;
        char *end = new_name;
        while (*end && *end != ' ' && *end != '\t') ++end;
        *end = '\0';

        if (!valid_name(new_name)) {
            send_text(c->fd, "Usage: /nick name (1-31 characters, no spaces).");
        } else if (name_in_use(new_name, c->fd)) {
            send_text(c->fd, "That nickname is already in use.");
        } else {
            char old_name[32], notice[160];
            snprintf(old_name, sizeof(old_name), "%s", c->name);
            snprintf(c->name, sizeof(c->name), "%s", new_name);
            snprintf(notice, sizeof(notice), "*** %s is now known as %s",
                     old_name, c->name);
            send_text(c->fd, "Nickname updated.");
            broadcast(notice, -1);
            log_message(notice);
        }
        return 0;
    }

    if (strcmp(line, "/list") == 0) {
        list_users(c->fd);
        return 0;
    }

    if (strncmp(line, "/msg ", 5) == 0) {
        char *target = line + 5;
        while (*target == ' ') ++target;
        char *space = strchr(target, ' ');
        if (!space || space[1] == '\0') {
            send_text(c->fd, "Usage: /msg username message");
            return 0;
        }
        *space++ = '\0';
        while (*space == ' ') ++space;

        size_t j;
        for (j = 0; j < client_count; ++j)
            if (strcmp(clients[j].name, target) == 0) break;

        if (j == client_count) {
            send_text(c->fd, "User not found.");
        } else {
            char message[MAX_LINE + 128];
            snprintf(message, sizeof(message), "[Private] %s: %s",
                     c->name, space);
            send_text(clients[j].fd, message);
            send_text(c->fd, message);
            log_message(message);
        }
        return 0;
    }

    if (strcmp(line, "/quit") == 0) return 1;

    if (line[0] == '/') {
        send_text(c->fd, "Unknown command. Use /nick, /msg, /list, /quit.");
        return 0;
    }

    char message[MAX_LINE + 64];
    snprintf(message, sizeof(message), "%s: %s", c->name, line);
    printf("%s\n", message);
    log_message(message);
    broadcast(message, -1);
    return 0;
}

static void show_logs(void) {
    FILE *fp = fopen(LOG_FILE, "r");
    if (!fp) {
        printf("No chat log yet (%s).\n", LOG_FILE);
        return;
    }
    printf("\n----- Chat history: %s -----\n", LOG_FILE);
    char line[1024];
    while (fgets(line, sizeof(line), fp)) fputs(line, stdout);
    printf("----- End of history -----\n");
    fclose(fp);
}

static void show_clients(void) {
    printf("Connected clients (%zu):", client_count);
    for (size_t i = 0; i < client_count; ++i)
        printf(" %s", clients[i].name);
    putchar('\n');
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }
    char *end = NULL;
    long port = strtol(argv[1], &end, 10);
    if (!end || *end || port < 1 || port > 65535) {
        fprintf(stderr, "Invalid port number.\n");
        return 1;
    }

    signal(SIGPIPE, SIG_IGN);
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int yes = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((unsigned short)port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind"); close(server_fd); return 1;
    }
    if (listen(server_fd, 10) < 0) {
        perror("listen"); close(server_fd); return 1;
    }

    printf("Chat server listening on port %ld.\n", port);
    printf("Admin commands: /logs, /clients, /shutdown\n");

    int running = 1;
    while (running) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        int max_fd = server_fd > STDIN_FILENO ? server_fd : STDIN_FILENO;

        for (size_t i = 0; i < client_count; ++i) {
            FD_SET(clients[i].fd, &read_fds);
            if (clients[i].fd > max_fd) max_fd = clients[i].fd;
        }

        int ready = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (ready < 0) {
            if (errno == EINTR) continue;
            perror("select"); break;
        }

        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            char command[128];
            if (!fgets(command, sizeof(command), stdin)) {
                clearerr(stdin);
            } else if (strncmp(command, "/logs", 5) == 0) {
                show_logs();
            } else if (strncmp(command, "/clients", 8) == 0) {
                show_clients();
            } else if (strncmp(command, "/shutdown", 9) == 0) {
                running = 0;
            } else if (command[0] != '\n') {
                printf("Admin commands: /logs, /clients, /shutdown\n");
            }
        }

        if (running && FD_ISSET(server_fd, &read_fds)) {
            struct sockaddr_in peer;
            socklen_t peer_len = sizeof(peer);
            int fd = accept(server_fd, (struct sockaddr *)&peer, &peer_len);
            if (fd >= 0) {
                if (fd >= FD_SETSIZE) {
                    send_text(fd, "Server capacity limit reached.");
                    close(fd);
                } else {
                    Client *grown = realloc(clients,
                                            (client_count + 1) * sizeof(Client));
                    if (!grown) {
                        send_text(fd, "Server is out of memory.");
                        close(fd);
                    } else {
                        clients = grown;
                        Client *c = &clients[client_count++];
                        memset(c, 0, sizeof(*c));
                        c->fd = fd;
                        snprintf(c->name, sizeof(c->name), "Guest%d", fd);
                        char notice[128];
                        snprintf(notice, sizeof(notice), "*** %s joined the chat",
                                 c->name);
                        printf("%s\n", notice);
                        log_message(notice);
                        send_text(fd, "Welcome! Commands: /nick name, /msg user message, /list, /quit.");
                        broadcast(notice, fd);
                    }
                }
            }
        }

        for (size_t i = 0; running && i < client_count; ) {
            if (!FD_ISSET(clients[i].fd, &read_fds)) {
                ++i;
                continue;
            }

            char incoming[1024];
            ssize_t n = recv(clients[i].fd, incoming, sizeof(incoming), 0);
            if (n <= 0) {
                remove_client(i, "disconnected");
                continue;
            }

            int disconnected = 0;
            for (ssize_t k = 0; k < n && !disconnected; ++k) {
                if (incoming[k] == '\n') {
                    clients[i].input[clients[i].used] = '\0';
                    if (clients[i].used > 0 &&
                        clients[i].input[clients[i].used - 1] == '\r')
                        clients[i].input[--clients[i].used] = '\0';
                    disconnected = handle_line(i, clients[i].input);
                    clients[i].used = 0;
                } else if (clients[i].used < sizeof(clients[i].input) - 1) {
                    clients[i].input[clients[i].used++] = incoming[k];
                } else {
                    send_text(clients[i].fd, "Message too long; disconnecting.");
                    disconnected = 1;
                }
            }

            if (disconnected) {
                remove_client(i, "left the chat");
            } else {
                ++i;
            }
        }
    }

    for (size_t i = 0; i < client_count; ++i) {
        send_text(clients[i].fd, "Server is shutting down.");
        close(clients[i].fd);
    }
    free(clients);
    close(server_fd);
    printf("Server stopped.\n");
    return 0;
}
