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

    char *server_ip = argv[1];
    int port = atoi(argv[2]);

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, server_ip, &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        return 1;
    }

    printf("Connected to server!\n");

    fd_set readfds;
    char buf[BUF_SIZE];

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sock, &readfds);

        int maxfd = (sock > STDIN_FILENO) ? sock : STDIN_FILENO;

        if (select(maxfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select");
            break;
        }

        // 📥 Nhận dữ liệu từ server
        if (FD_ISSET(sock, &readfds)) {
            int n = recv(sock, buf, sizeof(buf) - 1, 0);

            if (n <= 0) {
                printf("Disconnected from server\n");
                break;
            }

            buf[n] = 0;
            printf("\n%s\nYou: ", buf);
            fflush(stdout);
        }

        // ⌨️ Nhập từ bàn phím
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(buf, sizeof(buf), stdin) == NULL)
                break;

            send(sock, buf, strlen(buf), 0);

            printf("You: %s", buf);
        }
    }

    close(sock);
    return 0;
}