#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>

pollfd fds[21];
int fdCount = 1;


void sendToAll(char* message, int bytes) {
    for (int i = 1; i < fdCount; i++) {
        write(fds[i].fd, message, bytes);
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        printf("Niepoprawne wykonanie: %s <numer_portu>\n", argv[0]);
        return 1;
    }

    int servSock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr = {};
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(atoi(argv[1]));

    int one = 1;
    setsockopt(servSock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    if (bind(servSock, (sockaddr*) &serverAddr, sizeof(serverAddr)) == -1) {
        perror("Bind failed");
        close(servSock);
        return 1;
    }

    listen(servSock, SOMAXCONN);

    fds[0].fd = servSock;
    fds[0].events = POLLIN;

    while (true) {
        int ready = poll(fds, fdCount, -1);

        if (fds[0].revents & POLLIN) {
            sockaddr_in clientAddr = {};
            socklen_t clientAddrLen = sizeof(clientAddr);

            int clientSock = accept(servSock, (sockaddr*) &clientAddr, &clientAddrLen);

            printf("Nawiązano połączenie z: %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

            if (fdCount == 20) {
                write(clientSock, "Serwer pełny\n", 13);
                shutdown(clientSock, SHUT_RDWR);
                close(clientSock);
                continue;
            }

            fds[fdCount].fd = clientSock;
            fds[fdCount].events = POLLIN | POLLHUP;

            fdCount++;
        }

        for (int i = 0; i < fdCount; i++) {
            if (fds[i].revents & POLLIN) {
                char buf[255]{};

                int bytes = read(fds[i].fd, buf, 255);
                sendToAll(buf, bytes);
            }

            if (fds[i].revents & POLLHUP) {
                printf("Rozłączanie klienta numer %d\n", i);

                for (int j = i; j < fdCount; j++) {
                    fds[j] = fds[j + 1];
                }

                fdCount--;
            }
        }
    }

    close(servSock);
}