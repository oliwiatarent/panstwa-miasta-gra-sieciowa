#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <thread>
#include <vector>
#include <map>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <cstring>
#include <string>

class user{
    public: 
    bool active;
    std::string room;
    std::string username;
    bool username_set=false;
    user(){
        active = false;
    }
};

user users[21];
pollfd fds[21];
char buf[255]{};
int fdCount = 1;

void sendToAll(const char* message, int bytes) {
    for (int i = 1; i < fdCount; i++) {
        write(fds[i].fd, message, bytes);
    }
}

int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);

    if (argc != 2) {
        printf("Niepoprawne wykonanie: %s <numer_portu>\n", argv[0]);
        return 1;
    }

    std::map<int, std::string> responses;

    bool stop = false;
    bool endOfRound = false;

    int counter = 10;

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
    fds[0].events = POLLIN | POLLRDHUP;

    while (true) {
        int ready = poll(fds, fdCount, 1000);

        if (fds[0].revents & POLLIN) {
            char buf[255]{};
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

        for (int i = 1; i < fdCount; i++) {
            if (fds[i].revents & POLLIN) {
                if(users[i].username_set==false){
                    printf("test\n");
                    int bytes = read(fds[i].fd, buf, 255);
                    printf("%s\n",buf);
                    bool username_already_exists=false;
                      for(int i=1;i<fdCount;i++){
                          std::string pom;
                          pom.assign(buf,sizeof(buf));
                        if(pom.compare(users[i].username)==0){
                           username_already_exists=true;
                          break;
                        }
                    }
                    if(!username_already_exists){
                        users[i].username.assign(buf,sizeof(buf));
                        users[i].active=true;
                        printf("user %s added\n",users[i].username.c_str());
                        write(fds[i].fd, "OK", sizeof("OK"));
                        users[i].username_set=true;
                    }else{
                        write(fds[i].fd, "Username already in use", sizeof("Username already in use"));
                        printf("user tried already used username\n");
                    }
                }else{
                    int bytes = read(fds[i].fd, buf, 255);
                    if (strcmp(buf, "stop\n") == 0) {
                        stop = true;
                    } else {
                        if (responses.count(fds[i].fd) == 1);
                            //printf("Użytkownik %d już odpowiedział\n", fds[i].fd);
                       else 
                            responses.insert( {fds[i].fd, buf } );
                    }
                }
            }

                if (fds[i].revents &  POLLRDHUP) {
                    printf("Rozłączanie klienta numer %d\n", i);

                    shutdown(fds[i].fd, SHUT_RDWR);
                    close(fds[i].fd);

                    for (int j = i; j < fdCount; j++) {
                        fds[j] = fds[j + 1];
                        users[j] = users[j+1];
                    }

                    fdCount--;
                }
        }

        if (stop) {
            std::string msg = "time: " + std::to_string(counter) + "\n";
            sendToAll(msg.c_str(), msg.size());
            counter--;

            if (counter == -1) {
                endOfRound = true;
                stop = false;
                counter = 10;
            }
        }

        if (endOfRound) {
            for (auto response : responses) {
                std::string msg = std::to_string(response.first) + " | " + response.second;
                sendToAll(msg.c_str(), msg.size());
            }
            responses.clear();
            endOfRound = false;
        }
    }

    close(servSock);
}