#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <server_ip> <port>\n", argv[0]);
        return 1;
    }

    char *ip = argv[1];
    int port = atoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    fd_set readfds;
    char buf[BUF_SIZE];

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sock, &readfds);

        int maxfd = (sock > STDIN_FILENO) ? sock : STDIN_FILENO;

        if (select(maxfd+1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        // nhận từ server
        if (FD_ISSET(sock, &readfds)) {
            int n = recv(sock, buf, sizeof(buf)-1, 0);
            if (n <= 0) {
                printf("Disconnected\n");
                break;
            }

            buf[n] = 0;
            printf("%s", buf);
        }

        // nhập từ bàn phím
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(buf, sizeof(buf), stdin) == NULL)
                break;

            send(sock, buf, strlen(buf), 0);
        }
    }

    close(sock);
    return 0;
}