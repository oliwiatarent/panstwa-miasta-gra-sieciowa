#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Niepoprawne wykonanie: %s <numer_portu>\n", argv[0]);
        return 1;
    }

    char buf[255];

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr = {};
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(atoi(argv[1]));

    sockaddr_in clientAddr = {};
    socklen_t clientAddrSize = sizeof(clientAddr);

    if (bind(sock, (sockaddr*) &serverAddr, sizeof(serverAddr)) == -1) {
        perror("Bind failed");
        close(sock);
        return 1;
    }

    listen(sock, SOMAXCONN);

    while (true) {
        int client_sock = accept(sock, (sockaddr*) &clientAddr, &clientAddrSize);

        printf("Nawiązano połączenie z: %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

        int bytes = read(sock, buf, 255);
        write(1, buf, bytes);
        write(client_sock, "Response\n", 9);

        shutdown(client_sock, SHUT_RDWR);
        close(client_sock);
    }

    close(sock);
}