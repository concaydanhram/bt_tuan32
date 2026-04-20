#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <time.h>

#define MAX_CLIENTS  1024
#define BUF_SIZE     1024

typedef struct {
    int fd;
    char id[50];
    int has_id;
} client_t;

void get_time_str(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, size, "%Y/%m/%d %H:%M:%S", t);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    bind(listen_fd, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(listen_fd, 10);

    struct pollfd fds[MAX_CLIENTS];
    client_t clients[MAX_CLIENTS];

    memset(fds, 0, sizeof(fds));
    memset(clients, 0, sizeof(clients));

    fds[0].fd = listen_fd;
    fds[0].events = POLLIN;

    int nfds = 1;

    char buf[BUF_SIZE];

    while (1) {
        if (poll(fds, nfds, -1) < 0) {
            perror("poll");
            exit(1);
        }

        for (int i = 0; i < nfds; i++) {
            if (!(fds[i].revents & POLLIN)) continue;

            if (fds[i].fd == listen_fd) {
                int newfd = accept(listen_fd, NULL, NULL);

                send(newfd, "Enter ID (format: id: name): ", 32, 0);

                fds[nfds].fd = newfd;
                fds[nfds].events = POLLIN;

                clients[newfd].fd = newfd;
                clients[newfd].has_id = 0;

                nfds++;

                printf("New client connected (fd=%d)\n", newfd);
            }
            else {
                int fd = fds[i].fd;

                int nbytes = recv(fd, buf, sizeof(buf) - 1, 0);

                if (nbytes <= 0) {
                    printf("Client %d disconnected\n", fd);
                    close(fd);

                    for (int j = i; j < nfds - 1; j++) {
                        fds[j] = fds[j + 1];
                    }
                    nfds--;

                    memset(&clients[fd], 0, sizeof(client_t));
                    i--;
                    continue;
                }

                buf[nbytes] = 0;

                if (!clients[fd].has_id) {
                    char id[50], name[50];

                    if (sscanf(buf, "%[^:]: %s", id, name) == 2) {
                        strcpy(clients[fd].id, id);
                        clients[fd].has_id = 1;

                        send(fd, "ID accepted. You can chat now.\n", 33, 0);
                    } else {
                        send(fd, "Invalid format! Use: id: name\n", 32, 0);
                    }

                    continue;
                }

                char timebuf[64];
                get_time_str(timebuf, sizeof(timebuf));

                char msg[BUF_SIZE + 100];
                snprintf(msg, sizeof(msg),
                         "%s %s: %s",
                         timebuf,
                         clients[fd].id,
                         buf);

                for (int j = 0; j < nfds; j++) {
                    int dest = fds[j].fd;
                    if (dest != listen_fd && dest != fd) {
                        send(dest, msg, strlen(msg), 0);
                    }
                }

                printf("Broadcast: %s", msg);
            }
        }
    }

    return 0;
}