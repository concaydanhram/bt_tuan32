#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>

#define MAX_CLIENTS 1024
#define BUF_SIZE 1024

typedef struct {
    int fd;
    int state;
    char user[50];
} client_t;

int check_login(char *user, char *pass) {
    FILE *f = fopen("users.txt", "r");
    if (!f) return 0;

    char u[50], p[50];
    while (fscanf(f, "%s %s", u, p) != EOF) {
        if (strcmp(u, user) == 0 && strcmp(p, pass) == 0) {
            fclose(f);
            return 1;
        }
    }

    fclose(f);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(argv[1]));
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(listen_fd, 10);

    struct pollfd fds[MAX_CLIENTS];
    client_t clients[MAX_CLIENTS];

    for (int i = 0; i < MAX_CLIENTS; i++) {
        fds[i].fd = -1;
    }

    fds[0].fd = listen_fd;
    fds[0].events = POLLIN;

    int nfds = 1;
    char buf[BUF_SIZE];

    while (1) {
        poll(fds, nfds, -1);

        for (int i = 0; i < nfds; i++) {
            if (!(fds[i].revents & POLLIN)) continue;

            // accept new client
            if (fds[i].fd == listen_fd) {
                int newfd = accept(listen_fd, NULL, NULL);

                if (nfds >= MAX_CLIENTS) {
                    close(newfd);
                    continue;
                }

                fds[nfds].fd = newfd;
                fds[nfds].events = POLLIN;

                clients[nfds].fd = newfd;
                clients[nfds].state = 0;

                send(newfd, "Username:\n", 10, 0);
                nfds++;
            }
            else {
                int fd = fds[i].fd;
                int n = recv(fd, buf, sizeof(buf)-1, 0);

                if (n <= 0) {
                    close(fd);

                    for (int j = i; j < nfds - 1; j++) {
                        fds[j] = fds[j + 1];
                        clients[j] = clients[j + 1];
                    }

                    nfds--;
                    i--;
                    continue;
                }

                buf[n] = 0;
                buf[strcspn(buf, "\r\n")] = 0;

                if (clients[i].state == 0) {
                    strcpy(clients[i].user, buf);
                    clients[i].state = 1;
                    send(fd, "Password:\n", 10, 0);
                }
                else if (clients[i].state == 1) {
                    if (check_login(clients[i].user, buf)) {
                        clients[i].state = 2;
                        send(fd, "Login success!\n$ ", 18, 0);
                    } else {
                        send(fd, "Login failed!\nUsername:\n", 26, 0);
                        clients[i].state = 0;
                    }
                }
                else {
                    char cmd[256];
                    snprintf(cmd, sizeof(cmd), "%.200s > out.txt", buf);

                    system(cmd);

                    FILE *f = fopen("out.txt", "r");
                    if (!f) {
                        send(fd, "Command error\n$ ", 17, 0);
                        continue;
                    }

                    char line[BUF_SIZE];
                    while (fgets(line, sizeof(line), f)) {
                        send(fd, line, strlen(line), 0);
                    }

                    fclose(f);
                    send(fd, "$ ", 2, 0);
                }
            }
        }
    }
}