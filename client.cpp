#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
#include <cstring>

bool logged_in = false;
std::string current_room = "Start";
char username[255];

pollfd fds[2];


int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Niepoprawne wykonanie: %s <numer_portu>\n", argv[0]);
    }

    char* ip = argv[1];
    int port = atoi(argv[2]);
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr = {};
    serverAddr.sin_addr.s_addr = inet_addr(ip);;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (connect(sock, (sockaddr*) &serverAddr, sizeof(serverAddr)) == -1) {
        perror("Connect failed");
        close(sock);
        return -1;
    }

    fds[0].fd = 1;
    fds[0].events = POLLIN;
    fds[1].fd = sock;
    fds[1].events = POLLIN | POLLHUP | POLLERR;

    while (true) {
        int ready = poll(fds, 2, -1);
        char buf[255]{};
        bool disconnect = false;

        if (fds[1].revents & (POLLHUP | POLLERR)) {
            disconnect = true;
        }

        if ((fds[0].revents & POLLIN) && !disconnect) {
            int bytes = read(1, buf, 255);
            write(sock, buf, bytes);

            if (!logged_in) {
                strcpy(username, buf);
                username[strcspn(username, "\n")] = '\0';
            }
        }

        if ((fds[1].revents & POLLIN) && !disconnect) {
            int bytes = read(sock, buf, 255);

            if (!logged_in) {
                if (strcmp(buf, "Username available") == 0) {
                    logged_in = true;
                    printf("Zalogowano jako: %s\n", username);
                } else {
                    printf("Podana nazwa użytkownika jest już w użyciu. Spróbuj ponownie.\n");
                }

                continue;
            }

            printf("%s\n", buf);
            
            if (strcmp(buf, "New Room Created") == 0) {
                // tutaj dodanie do pokoju
                printf("test new room created\n");

                continue;
            }

            if (strcmp(buf, "Joining Room") == 0) {
                // tutaj dodanie do pokoju
                printf("test joining room\n");

                continue;
            }
        }

        if (disconnect) {
            printf("Nastąpiło rozłączenie z serwerem\n");

            shutdown(sock, SHUT_RDWR);
            close(sock);

            exit(0);
        }
    }

    shutdown(sock, SHUT_RDWR);
    close(sock);
}
