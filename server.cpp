#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <map>

#define MAX_CLIENTS 99999

std::map<int, std::vector<std::string>> responses;

bool stop = false;
bool endOfRound = false;

int counter = 10;

class user{
    public: 
    bool active;
    std::string room;
    std::string CustomRoom;
    std::string username;
    std::vector<std::string> recv;
    bool username_set=false;
    bool choosing_room_name = false;
    user(){
        active = false;
        room="Start";
        CustomRoom="";
    }
};

class gameroom{
    public:
    user owner;
    user players[10];
    int NumberOfPlayers=1;
    std::string RoomName;
};

user users[MAX_CLIENTS];
pollfd fds[MAX_CLIENTS];
char buf[255]{};
int fdCount = 1;
int NumberOfUsers=1;
int NumberOfRooms=1;
gameroom GameRooms[10];

void sendToAll(const char* message, int bytes) {
    for (int i = 1; i < fdCount; i++) {
        write(fds[i].fd, message, bytes);
    }
}

std::vector<std::string> responseToVector(char* buf) {
    std::vector<std::string> answers;
    std::string answer;

    int i = 0;
    while (true) {
        if (buf[i] == ' ') {
            answers.push_back(answer);
            answer = "";
        } else if (buf[i] == '\n') {
            answers.push_back(answer);
            break;
        } else {
            answer += buf[i];
        }
        i++;
    }

    return answers;
}

int main(int argc, char** argv) {
    signal(SIGPIPE, SIG_IGN);

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
    fds[0].events = POLLIN | POLLHUP;

    while (true) {
        int ready = poll(fds, fdCount, 1000);

        if (fds[0].revents & POLLIN) {
            char buf[255]{};
            sockaddr_in clientAddr = {};
            socklen_t clientAddrLen = sizeof(clientAddr);

            int clientSock = accept(servSock, (sockaddr*) &clientAddr, &clientAddrLen);

            if (clientSock == -1) {
                printf("Wystąpił błąd przy akceptowaniu połączenia\n");
                continue;
            }

            printf("Nawiązano połączenie z: %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

            fds[fdCount].fd = clientSock;
            fds[fdCount].events = POLLIN | POLLHUP | POLLERR;

            fdCount++;
            NumberOfUsers++;
        }

        for (int i = 1; i < fdCount; i++) {

            bool disconnect = false;

            if (fds[i].revents & (POLLHUP | POLLERR)) {
                disconnect = true;
            }

            if ((fds[i].revents & POLLIN) && !disconnect) {
                char buf[255]{};
                int bytes = read(fds[i].fd, buf, 255);

                if (bytes <= 0) {
                    disconnect = true;
                } else {

                    if(users[i].username_set==false){

                        bool username_already_exists=false;

                        for(int i=1;i<fdCount;i++){
                            std::string pom;
                            pom.assign(buf,sizeof(buf));
                            if(pom.compare(users[i].username)==0){
                                username_already_exists=true;
                                break;
                            }
                        }

                        if (!username_already_exists) {
                            users[i].username=responseToVector(buf)[0];
                            users[i].active=true;
                            printf("user added: %s\n",users[i].username.c_str());
                            write(fds[i].fd, "OK\n", sizeof("OK\n"));
                            users[i].username_set=true;
                        } else {
                            write(fds[i].fd, "Username already in use\n", sizeof("Username already in use\n"));
                            printf("user tried already used username\n");
                        }

                    } else {

                        std::vector<std::string> response = responseToVector(buf);
                        users[i].recv = response;

                        if (strcmp(buf, "stop\n") == 0) {
                            stop = true;
                        } else {
                            if (users[i].room.compare("Start")==0){
                                if (users[i].recv[0].compare("CreateNewRoom") == 0) {

                                    if (response.size() == 2) {

                                        printf("New room created: %s\n", response[1].c_str());
                                        write(fds[i].fd, "NewRoomCreated\n", sizeof("NewRoomCreated\n"));

                                        users[i].room = "CustomRoom";
                                        users[i].CustomRoom = users[i].recv[1];
                                        GameRooms[NumberOfRooms].RoomName = users[i].recv[1];
                                        GameRooms[NumberOfRooms].players[GameRooms[NumberOfRooms].NumberOfPlayers]=users[i];
                                        GameRooms[NumberOfRooms].NumberOfPlayers++;
                                        NumberOfRooms++;

                                    } else {
                                        printf("Incorrect command\n");
                                    }

                                } else if(users[i].recv[0].compare("JoinRoom") == 0){
                                    if (response.size() == 2) {

                                        write(fds[i].fd, "Joining Room\n", sizeof("Joining Room\n"));

                                        for(int j=1;j<NumberOfRooms;j++){
                                            if(strcmp(GameRooms[j].RoomName.c_str(), users[i].recv[1].c_str())==0){
                                                GameRooms[j].players[GameRooms[j].NumberOfPlayers]=users[i];
                                                GameRooms[j].NumberOfPlayers++;
                                                printf("Joined Room: %s\n", users[i].recv[1].c_str());
                                                users[i].room = "CustomRoom";
                                                users[i].CustomRoom = users[i].recv[1];
                                            }
                                        }

                                    } else {
                                        printf("Incorrect command\n");
                                    }
                                }else {
                                    responses.insert( {fds[i].fd, response} );
                                }
                            }else if(strcmp(users[i].room.c_str(),"CustomRoom")==0){
                                if (users[i].recv[0].compare("AddPlayerToRoom") == 0) {

                                    if (response.size() == 2) {

                                        printf("Player added: %s\n", users[i].recv[1].c_str());
                                        write(fds[i].fd, "AddedPlayer\n", sizeof("AddedPlayer\n"));
                                        
                                        for(int j=1;j<NumberOfRooms;j++){
                                            if(strcmp(users[i].CustomRoom.c_str(), GameRooms[j].RoomName.c_str())==0){
                                                printf("Room exists!\n");
                                                for(int k=1;k<NumberOfUsers;k++){
                                                    printf("%d\n",NumberOfUsers);
                                                    printf("searching for player... %s %s\n", users[i].recv[1].c_str(),users[k].username.c_str());
                                                    if(strcmp(users[i].recv[1].c_str(),users[k].username.c_str())==0){
                                                        GameRooms[j].players[GameRooms[j].NumberOfPlayers]=users[k];
                                                        GameRooms[j].NumberOfPlayers++;
                                                        users[k].room = "CustomRoom";
                                                        users[k].CustomRoom = GameRooms[k].RoomName;
                                                        printf("success!!!!\n");
                                                    }
                                                }
                                            }
                                        }
                                        

                                    } else {
                                        printf("Incorrect command\n");
                                    }

                                } else {
                                    responses.insert( {fds[i].fd, response} );
                                }
                            }
                        }
                        }
                }
            }

            if (disconnect) {
                printf("Rozłączanie klienta numer %d\n", i);

                shutdown(fds[i].fd, SHUT_RDWR);
                close(fds[i].fd);

                for (int j = i; j < fdCount; j++) {
                    fds[j] = fds[j + 1];
                    users[j] = users[j+1];
                }

                fdCount--;
                i--;
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
                std::string msg = std::to_string(response.first) + " | ";
                for (std::string answer : response.second) {
                    msg += answer + " ";
                }
                msg += "\n";
                sendToAll(msg.c_str(), msg.size());
            }
            responses.clear();
            endOfRound = false;
        }
    }

    close(servSock);
}