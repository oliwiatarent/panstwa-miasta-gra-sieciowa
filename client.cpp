#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

int main(int argc, char** argv) {
    if (argc != 3) {
        printf("Niepoprawne wykonanie: %s <adres_ip> <numer_portu>\n", argv[0]);
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in clientAddr = {};
    clientAddr.sin_addr.s_addr = INADDR_ANY;
    clientAddr.sin_family = AF_INET;

    if (bind(sock, (sockaddr*) &clientAddr, sizeof(clientAddr)) == -1) {
        perror("Bind failed");
        close(sock);
        return 1;
    }

    sockaddr_in serverAddr = {};
    serverAddr.sin_addr.s_addr = inet_addr(argv[1]);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(atoi(argv[2]));

    if (connect(sock, (sockaddr*) &serverAddr, sizeof(serverAddr)) == -1) {
        perror("Connect failed");
        close(sock);
        return 1;
    }

    char buf[255];

    write(sock, "Request\n", 8);
    int bytes = read(sock, buf, 255);
    write(1, buf, bytes);

    shutdown(sock, SHUT_RDWR);
    close(sock);
}