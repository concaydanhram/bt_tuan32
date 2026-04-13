#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define MAX_CLIENTS FD_SETSIZE
#define BUF_SIZE 1024

typedef struct {
    int fd;
    int state; // 0: user, 1: pass, 2: logged in
    char user[50];
} client_t;

// check login từ file users.txt
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

    int port = atoi(argv[1]);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(listen_fd, 10);

    fd_set master, readfds;
    FD_ZERO(&master);
    FD_SET(listen_fd, &master);

    int fdmax = listen_fd;
    client_t clients[MAX_CLIENTS] = {0};

    char buf[BUF_SIZE];

    while (1) {
        readfds = master;

        if (select(fdmax+1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            exit(1);
        }

        for (int i = 0; i <= fdmax; i++) {
            if (!FD_ISSET(i, &readfds)) continue;

            // 🆕 client mới
            if (i == listen_fd) {
                int newfd = accept(listen_fd, NULL, NULL);

                FD_SET(newfd, &master);
                if (newfd > fdmax) fdmax = newfd;

                clients[newfd].fd = newfd;
                clients[newfd].state = 0;

                send(newfd, "Username:\n", 10, 0);
            }
            else {
                int n = recv(i, buf, sizeof(buf)-1, 0);

                if (n <= 0) {
                    close(i);
                    FD_CLR(i, &master);
                    memset(&clients[i], 0, sizeof(client_t));
                    continue;
                }

                buf[n] = 0;
                buf[strcspn(buf, "\r\n")] = 0;
                if (clients[i].state == 0) {
                    strcpy(clients[i].user, buf);
                    clients[i].state = 1;
                    send(i, "Password:\n", 10, 0);
                }
                else if (clients[i].state == 1) {
                    if (check_login(clients[i].user, buf)) {
                        clients[i].state = 2;
                        send(i, "Login success!\n$ ", 18, 0);
                    } else {
                        send(i, "Login failed!\nUsername:\n", 26, 0);
                        clients[i].state = 0;
                    }
                }
                else {
                    // thực thi lệnh
                    char cmd[256];
                    snprintf(cmd, sizeof(cmd), "%.200s > out.txt", buf);

                    system(cmd);

                    FILE *f = fopen("out.txt", "r");
                    if (!f) {
                        send(i, "Command error\n$ ", 17, 0);
                        continue;
                    }

                    char line[BUF_SIZE];
                    while (fgets(line, sizeof(line), f)) {
                        send(i, line, strlen(line), 0);
                    }

                    fclose(f);
                    send(i, "$ ", 2, 0);
                }
            }
        }
    }
}