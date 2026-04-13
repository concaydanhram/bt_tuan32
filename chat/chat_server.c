#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <time.h>

#define MAX_CLIENTS  FD_SETSIZE
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

    fd_set master, readfds;
    FD_ZERO(&master);
    FD_SET(listen_fd, &master);

    int fdmax = listen_fd;

    client_t clients[MAX_CLIENTS] = {0};

    char buf[BUF_SIZE];

    while (1) {
        readfds = master;

        if (select(fdmax + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            exit(1);
        }

        for (int i = 0; i <= fdmax; i++) {
            if (!FD_ISSET(i, &readfds)) continue;

            // 🆕 New connection
            if (i == listen_fd) {
                int newfd = accept(listen_fd, NULL, NULL);
                send(newfd, "Enter ID (format: id: name): ", 32, 0);

                FD_SET(newfd, &master);
                if (newfd > fdmax) fdmax = newfd;

                clients[newfd].fd = newfd;
                clients[newfd].has_id = 0;

                printf("New client connected (fd=%d)\n", newfd);
            }
            else {
                int nbytes = recv(i, buf, sizeof(buf) - 1, 0);

                if (nbytes <= 0) {
                    printf("Client %d disconnected\n", i);
                    close(i);
                    FD_CLR(i, &master);
                    memset(&clients[i], 0, sizeof(client_t));
                    continue;
                }

                buf[nbytes] = 0;

                if (!clients[i].has_id) {
                    char id[50], name[50];

                    if (sscanf(buf, "%[^:]: %s", id, name) == 2) {
                        strcpy(clients[i].id, id);
                        clients[i].has_id = 1;

                        send(i, "ID accepted. You can chat now.\n", 33, 0);
                    } else {
                        send(i, "Invalid format! Use: id: name\n", 32, 0);
                    }

                    continue;
                }

                // 📨 Broadcast
                char timebuf[64];
                get_time_str(timebuf, sizeof(timebuf));

                char msg[BUF_SIZE + 100];
                snprintf(msg, sizeof(msg),
                         "%s %s: %s",
                         timebuf,
                         clients[i].id,
                         buf);

                for (int j = 0; j <= fdmax; j++) {
                    if (FD_ISSET(j, &master) && j != listen_fd && j != i) {
                        send(j, msg, strlen(msg), 0);
                    }
                }

                printf("Broadcast: %s", msg);
            }
        }
    }

    return 0;
}