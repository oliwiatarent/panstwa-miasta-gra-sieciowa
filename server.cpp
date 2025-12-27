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
#include <chrono>

#define MAX_CLIENTS 99999

std::map<int, std::vector<std::string>> responses;

bool stop = false;
bool endOfRound = false;

int counter = 10;

class user{
    public: 
    int points=0;
    bool active;
    std::string word[5];
    std::string room;
    std::string CustomRoom;
    std::string username;
    std::vector<std::string> recv;
    bool username_set=false;
    bool choosing_room_name = false;
    bool InActiveGame = false;
    user(){
        active = false;
        room="Start";
        CustomRoom="";
    }
};

class gameroom{
    public:
    std::chrono::_V2::system_clock::time_point StartTime;
    std::chrono::_V2::system_clock::time_point StopTime;
    bool EndGame = false;
    int TimeLimit=60000;
    int StopLimit=10000;
    char GameLetter='A';
    user owner;
    int players[10];
    int NumberOfPlayers=1;
    std::string RoomName;
    bool ActiveGame = false;
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

                    std::vector<std::string> response = responseToVector(buf);

                    if(users[i].username_set==false){

                        bool username_already_exists=false;
                        std::string pom = response[0];

                        for(int i=1;i<fdCount;i++){
                            
                            if(strcmp(users[i].username.c_str(), pom.c_str())==0){
                                username_already_exists=true;
                                break;
                            }
                        }

                        if (!username_already_exists) {
                            users[i].username=responseToVector(buf)[0];
                            users[i].active=true;
                            printf("user added: %s\n",users[i].username.c_str());
                            write(fds[i].fd, "Username available", sizeof("Username available"));
                            users[i].username_set=true;
                        } else {
                            write(fds[i].fd, "Username already in use", sizeof("Username already in use"));
                            printf("user tried already used username\n");
                        }

                    } else {

                        users[i].recv = response;

                        if (strcmp(buf, "stop\n") == 0) {
                            stop = true;
                        } else {
                            if (users[i].room.compare("Start")==0){

                                if (strcmp(users[i].recv[0].c_str(), "CreateNewRoom") == 0) {

                                    if (response.size() == 2) {

                                        printf("New room created: %s\n", response[1].c_str());
                                        write(fds[i].fd, "New Room Created", sizeof("New Room Created"));

                                        users[i].room = "CustomRoom";
                                        users[i].CustomRoom = users[i].recv[1];
                                        GameRooms[NumberOfRooms].RoomName = users[i].recv[1];
                                        GameRooms[NumberOfRooms].players[GameRooms[NumberOfRooms].NumberOfPlayers]=i;
                                        GameRooms[NumberOfRooms].NumberOfPlayers++;
                                        NumberOfRooms++;

                                    } else {
                                        printf("Incorrect command\n");
                                    }

                                } else if(strcmp(users[i].recv[0].c_str(), "JoinRoom") == 0){
                                    
                                    if (response.size() == 2) {
                                        
                                        printf("Joining room\n");

                                        for(int j=1;j<NumberOfRooms;j++){
                                            if(strcmp(GameRooms[j].RoomName.c_str(), users[i].recv[1].c_str())==0){
                                                write(fds[i].fd, "Joining Room", sizeof("Joining Room"));
                                                GameRooms[j].players[GameRooms[j].NumberOfPlayers]=i;
                                                GameRooms[j].NumberOfPlayers++;
                                                printf("Joined Room: %s\n", users[i].recv[1].c_str());
                                                users[i].room = "CustomRoom";
                                                users[i].CustomRoom = users[i].recv[1];
                                            }
                                        }

                                    } else {
                                        printf("Incorrect command\n");
                                    }
                                }
                            }else if(strcmp(users[i].room.c_str(),"CustomRoom")==0 && users[i].InActiveGame == false){
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
                                                        GameRooms[j].players[GameRooms[j].NumberOfPlayers]=k;
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

                                }else if(strcmp(users[i].recv[0].c_str(), "StartGame") == 0){
                                    users[i].InActiveGame = true;
                                    int RoomIndex=-1;
                                    for(int j=1;j<NumberOfRooms;j++){
                                        if(strcmp(GameRooms[j].RoomName.c_str(),users[i].CustomRoom.c_str())==0){
                                            GameRooms[j].ActiveGame = true;
                                            RoomIndex=j;
                                        }
                                    }
                                    if(RoomIndex != -1){
                                        GameRooms[RoomIndex].GameLetter = 'A' + rand()%26;
                                        printf("%c\n",GameRooms[RoomIndex].GameLetter);
                                        for(int j=1;j<NumberOfUsers;j++){
                                            if(strcmp(users[j].CustomRoom.c_str(),GameRooms[RoomIndex].RoomName.c_str())==0){
                                                users[j].InActiveGame=true;
                                                write(fds[j].fd,"Your game started",sizeof("Your game started"));
                                            }
                                        }
                                        GameRooms[RoomIndex].StartTime = std::chrono::system_clock::now();
                                        printf("activeted game for room and all players\n");
                                    }
                                }
                            }else if(strcmp(users[i].recv[0].c_str(),"SendAnswers")==0 && users[i].InActiveGame == true){
                                    printf("got something\n");
                                    int RoomIndex=-1;
                                    for(int j=1;j<NumberOfRooms;j++){
                                        if(strcmp(GameRooms[j].RoomName.c_str(),users[i].CustomRoom.c_str())==0){
                                            GameRooms[j].ActiveGame = true;
                                            RoomIndex=j;
                                        }
                                    }
                                    //std::chrono::_V2::system_clock::time_point CurrentTime = std::chrono::system_clock::now();
                                    if(response.size()<6){
                                        for(int j=1;j<response.size();j++){
                                            users[i].word[j-1]=users[i].recv[j];
                                            if(!GameRooms[RoomIndex].EndGame){
                                                GameRooms[RoomIndex].EndGame=true;
                                                GameRooms[RoomIndex].StopTime = std::chrono::system_clock::now();
                                            }
                                        }
                                    }else{
                                        write(fds[i].fd,"bad answers",sizeof("bad answers"));
                                    }

                                    printf("user %s gave answers: \n",users[i].username.c_str());
                                    for(int j=0;j<response.size()-1;j++){
                                        printf("%s\n",users[i].word[j].c_str());
                                    }
                                }else{
                                responses.insert( {fds[i].fd, response} );
                            }
                        }
                    }
                }
            }

            if (disconnect) {
                printf("Rozłączanie klienta numer %d\n", fds[i].fd);

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

        for(int i=1;i<NumberOfRooms;i++){
            if(GameRooms[i].ActiveGame)if(GameRooms[i].TimeLimit < (int)((std::chrono::system_clock::now() - GameRooms[i].StartTime).count()/1000000) || (GameRooms[i].EndGame == true && GameRooms[i].StopLimit < (int)((std::chrono::system_clock::now() - GameRooms[i].StopTime).count()/1000000))){
                int MaxPoints=0;
                std::vector <std::string> winners;
                for(int j=1;j<GameRooms[i].NumberOfPlayers;j++){
                    users[GameRooms[i].players[j]].InActiveGame = false;
                    for(int k=0;k<5;k++){
                        if(users[GameRooms[i].players[j]].word[k][0] == GameRooms[i].GameLetter){
                            bool p20=true;
                            for(int l=0;l<GameRooms[i].NumberOfPlayers;l++){
                                if(l!=GameRooms[i].players[j]){
                                    if(strcmp(users[GameRooms[i].players[j]].word[k].c_str(),users[l].word[k].c_str())==0){
                                        p20=false;
                                        users[GameRooms[i].players[j]].points+=10;
                                        break;
                                    }
                                }
                            }
                            if(p20){
                                users[GameRooms[i].players[j]].points+=20;
                            }
                        }else{
                            printf("expected %c got %s\n",GameRooms[i].GameLetter,users[GameRooms[i].players[j]].word[k].c_str());
                        }
                    }
                    printf("player %s got %d points\n", users[GameRooms[i].players[j]].username.c_str(),users[GameRooms[i].players[j]].points);
                    if(users[GameRooms[i].players[j]].points>MaxPoints){
                        MaxPoints = users[GameRooms[i].players[j]].points;
                        winners.clear();
                        winners.push_back(users[GameRooms[i].players[j]].username);
                    }else if(users[GameRooms[i].players[j]].points == MaxPoints){
                        winners.push_back(users[GameRooms[i].players[j]].username);
                    }
                }
                if(winners.size()>1){
                    printf("winners:\n");
                }else{
                    printf("winner:\n");
                }
                for(int k=0;k<winners.size();k++){
                    printf("%s\n",winners[k].c_str());
                }
                GameRooms[i].ActiveGame=false;
                GameRooms[i].EndGame=false;
                for(int j=1;j<GameRooms[i].NumberOfPlayers;j++){
                    users[GameRooms[i].players[j]].points=0;
                    for(int k=0;k<5;k++){
                        users[GameRooms[i].players[j]].word[k]="";
                    }
                }
            }
        }
    }

    close(servSock);
}