#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <netdb.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstring>
#include <string>
#include <thread>
#include <vector>
#include <map>
#include <chrono>
#include <sstream>

#define MAX_CLIENTS 99999
#define MAX_GAMEROOMS 10

std::map<int, std::vector<std::string>> responses;

class user
{
public:
    int TimePoints = 0;
    int points = 0;
    int GamePoints = 0;
    bool active;
    std::string word[5];
    std::string room;
    std::string CustomRoom;
    std::string username;
    std::vector<std::string> recv;
    std::string inputBuff;  // do buforowania reada
    bool username_set = false;
    bool choosing_room_name = false;
    bool InActiveGame = false;
    user()
    {
        active = false;
        room = "Start";
        CustomRoom = "";
    }
};

class gameroom
{
public:
    bool CountingDownToStart = false;
    bool StartAgain = true;
    std::chrono::system_clock::time_point ThreePlayersEnteredTime;
    std::chrono::system_clock::time_point StartTime;
    std::chrono::system_clock::time_point StopTime;
    std::chrono::system_clock::time_point ContinueGameTimer;
    bool EndGame = false;
    int TimeLimit = 60000;
    int ContinueGameTimeLimit = 30000;
    int GameStartLimit = 30000;
    int StopLimit = 10000;
    char GameLetter = 'A';
    int owner;
    int players[10];
    int NumberOfPlayers = 0;
    std::string RoomName;
    bool ActiveGame = false;
    int RoundsLimit = 2;
    int RoundsLeft = 0;
    bool ContinueGame = false;
};

user users[MAX_CLIENTS];
pollfd fds[MAX_CLIENTS];
char buf[255]{};
int fdCount = 1;
int NumberOfUsers = 1;
int NumberOfRooms = 1;
gameroom GameRooms[MAX_GAMEROOMS];

void sendToAllInRoom(const char *message, int bytes, std::string roomName)
{

    for (int i = 1; i < NumberOfUsers; i++)
    {
        user &u = users[i];

        if (strcmp(u.CustomRoom.c_str(), roomName.c_str()) == 0)
        {
            write(fds[i].fd, message, bytes);
        }
    }
}

void sendRoomInformationInLobby()
{
    for (int i = 1; i < NumberOfUsers; i++)
    {

        user &u = users[i];
        if (strcmp(u.room.c_str(), "Start") == 0 && u.username_set)
        {
            // header dla klientów
            std::string header = "ROOMLIST_START\n";
            write(fds[i].fd, header.c_str(), header.size());
            for (int j = 1; j < NumberOfRooms; j++)
            {
                gameroom &g = GameRooms[j];
                // dla gui: ROOM:Nazwa:LGraczy:Status
                std::string msg = "ROOM:" + g.RoomName + ":" + std::to_string(g.NumberOfPlayers) + ":" + (g.ActiveGame ? "Active" : "Inactive") + "\n";
                //std::string msg = g.RoomName + " | " + std::to_string(g.NumberOfPlayers) + " | " + (g.ActiveGame ? "Active" : "Inactive");
                write(fds[i].fd, msg.c_str(), msg.size());
            }
        }
    }
}
void StartGame(int i)
{
    users[i].InActiveGame = true;
    int RoomIndex = -1;
    for (int j = 0; j < NumberOfRooms; j++)
    {
        if (strcmp(GameRooms[j].RoomName.c_str(), users[i].CustomRoom.c_str()) == 0)
        {
            GameRooms[j].ActiveGame = true;
            RoomIndex = j;
        }
    }
    if (RoomIndex != -1)
    {
        GameRooms[RoomIndex].GameLetter = 'A' + rand() % 26;
        printf("%c\n", GameRooms[RoomIndex].GameLetter);

        for (int j = 1; j < NumberOfUsers; j++)
        {
            if (strcmp(users[j].CustomRoom.c_str(), GameRooms[RoomIndex].RoomName.c_str()) == 0)
            {
                users[j].InActiveGame = true;
                write(fds[j].fd, "Your game started\n", sizeof("Your game started\n"));
            }
        }
        GameRooms[RoomIndex].StartTime = std::chrono::system_clock::now();
        printf("activeted game for room and all players\n");

        std::string msg = "Litera: " + std::string(1, GameRooms[RoomIndex].GameLetter);
        sendToAllInRoom(msg.c_str(), msg.size(), GameRooms[RoomIndex].RoomName);
    }
}

void sendRoomInformation()
{
    for (int i = 1; i < NumberOfRooms; i++)
    {
        gameroom &g = GameRooms[i];
        std::string msg;

        if (!g.ActiveGame)
        {

            for (int j = 1; j < NumberOfUsers; j++)
            {
                user &u = users[j];

                if (strcmp(u.CustomRoom.c_str(), g.RoomName.c_str()) == 0)
                {
                    msg += u.username;

                    if (j != NumberOfUsers - 1)
                        msg += " | ";
                }
            }

            msg += "\n";

            sendToAllInRoom(msg.c_str(), msg.size(), g.RoomName);
        }
    }
}

int findroom(std::string s)
{
    for (int j = 1; j < NumberOfRooms; j++)
    {
        if (strcmp(GameRooms[j].RoomName.c_str(), s.c_str()) == 0)
        {
            return j;
        }
    }
    return -1;
}

std::vector<std::string> responseToVector(std::string input)
{
    std::vector<std::string> answers;
    std::string answer;
    std::stringstream ss(input);

    while (std::getline(ss, answer, ' ')) {
        if (!answer.empty()) {
            // usuwa znak \n albo \r z bufora
            if (answer.back() == '\n')
                answer.pop_back();
            if (!answer.empty() && answer.back() == '\r')
                answer.pop_back();
            if(!answer.empty())
                answers.push_back(answer);
        }
    }
    return answers;
}

int main(int argc, char **argv)
{
    signal(SIGPIPE, SIG_IGN);

    if (argc != 2)
    {
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

    if (bind(servSock, (sockaddr *)&serverAddr, sizeof(serverAddr)) == -1)
    {
        perror("Bind failed");
        close(servSock);
        return 1;
    }

    listen(servSock, SOMAXCONN);

    fds[0].fd = servSock;
    fds[0].events = POLLIN | POLLHUP;

    while (true)
    {
        int ready = poll(fds, fdCount, 1000);

        if (fds[0].revents & POLLIN)
        {
            char buf[255]{};
            sockaddr_in clientAddr = {};
            socklen_t clientAddrLen = sizeof(clientAddr);

            int clientSock = accept(servSock, (sockaddr *)&clientAddr, &clientAddrLen);

            if (clientSock == -1)
            {
                printf("Wystąpił błąd przy akceptowaniu połączenia\n");
                continue;
            }

            printf("Nawiązano połączenie z: %s:%d\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));

            fds[fdCount].fd = clientSock;
            fds[fdCount].events = POLLIN | POLLHUP | POLLERR;

            fdCount++;
            NumberOfUsers++;
        }

        for (int i = 1; i < fdCount; i++)
        {

            bool disconnect = false;

            if (fds[i].revents & (POLLHUP | POLLERR))
            {
                disconnect = true;
            }

            if ((fds[i].revents & POLLIN) && !disconnect)
            {
                char buf[256]{};
                int bytes = read(fds[i].fd, buf, 255);

                if (bytes <= 0)
                {
                    disconnect = true;
                }
                else
                {
                    users[i].inputBuff.append(buf, bytes);

                    auto pos = 0;
                    // póki znajdujemy znak końca komendy
                    while ((pos = users[i].inputBuff.find('\n')) != std::string::npos) {
                        std::string fullCommand = users[i].inputBuff.substr(0, pos);
                        users[i].inputBuff.erase(0, pos+1); // +1 bo z \n

                        if (fullCommand.empty())
                            continue;

                        std::vector<std::string> response = responseToVector(fullCommand);

                        if (response.empty())
                            continue;

                        if (users[i].username_set == false)
                        {

                            bool username_already_exists = false;
                            std::string pom = response[0];

                            for (int i = 1; i < fdCount; i++)
                            {

                                if (strcmp(users[i].username.c_str(), pom.c_str()) == 0)
                                {
                                    username_already_exists = true;
                                    break;
                                }
                            }

                            if (!username_already_exists)
                            {
                                users[i].username = responseToVector(buf)[0];
                                users[i].active = true;
                                printf("user added: %s\n", users[i].username.c_str());
                                write(fds[i].fd, "Username available\n", sizeof("Username available\n"));
                                users[i].username_set = true;
                            }
                            else
                            {
                                write(fds[i].fd, "Username already in use\n", sizeof("Username already in use\n"));
                                printf("user tried already used username\n");
                            }
                        }
                        else
                        {

                            users[i].recv = response;

                            if (users[i].room.compare("Start") == 0)
                            {

                                if (strcmp(users[i].recv[0].c_str(), "CreateNewRoom") == 0)
                                {

                                    if (response.size() == 2)
                                    {

                                        printf("New room created: %s\n", response[1].c_str());
                                        write(fds[i].fd, "New Room Created\n", sizeof("New Room Created\n"));

                                        users[i].room = "CustomRoom";
                                        users[i].CustomRoom = users[i].recv[1];
                                        GameRooms[NumberOfRooms].RoomName = users[i].recv[1];
                                        GameRooms[NumberOfRooms].players[GameRooms[NumberOfRooms].NumberOfPlayers] = i;
                                        GameRooms[NumberOfRooms].NumberOfPlayers++;
                                        GameRooms[NumberOfRooms].owner = i;
                                        // do wyświetlania listy graczy
                                        std::string msg = "Points:" + users[i].username + ":0\n";
                                        write(fds[i].fd, msg.c_str(), msg.size());
                                        NumberOfRooms++;
                                    }
                                    else
                                    {
                                        printf("Incorrect command\n");
                                    }
                                }
                                else if (strcmp(users[i].recv[0].c_str(), "JoinRoom") == 0)
                                {

                                    if (response.size() == 2)
                                    {

                                        printf("Joining room\n");
                                        bool JoinedRoom = false;

                                        for (int j = 1; j < NumberOfRooms; j++)
                                        {
                                            if (GameRooms[j].ActiveGame == false && strcmp(GameRooms[j].RoomName.c_str(), users[i].recv[1].c_str()) == 0)
                                            {
                                                JoinedRoom = true;
                                                write(fds[i].fd, "Joining Room\n", sizeof("Joining Room\n"));
                                                GameRooms[j].players[GameRooms[j].NumberOfPlayers] = i;
                                                GameRooms[j].NumberOfPlayers++;
                                                printf("Joined Room: %s\n", users[i].recv[1].c_str());
                                                users[i].room = "CustomRoom";
                                                users[i].CustomRoom = users[i].recv[1];
                                                // do wyświetlania listy graczy
                                                std::string msg = "Points:" + users[i].username + ":0\n";
                                                sendToAllInRoom(msg.c_str(), msg.size(), GameRooms[j].RoomName);
                                                for (int k = 0; k < GameRooms[j].NumberOfPlayers; k++)
                                                {
                                                    int player = GameRooms[j].players[k];
                                                    if (player != i)
                                                    {
                                                        std::string msg = "Points:" + users[player].username + ":0\n";
                                                        write(fds[i].fd, msg.c_str(), msg.size());
                                                    }
                                                }
                                            }
                                        }
                                        if (!JoinedRoom)
                                        {
                                            write(fds[i].fd, "Failed to join a room\n", sizeof("Failed to join a room\n"));
                                            printf("Failed to join a room\n");
                                        }
                                    }
                                    else
                                    {
                                        printf("Incorrect command\n");
                                    }
                                }
                            }
                            else if (strcmp(users[i].room.c_str(), "CustomRoom") == 0 && GameRooms[findroom(users[i].CustomRoom)].ActiveGame == true)
                            {
                                int RoomIndex = findroom(users[i].CustomRoom);

                                if (strcmp(users[i].recv[0].c_str(), "SetRoundLimit") == 0 && strcmp(users[i].username.c_str(), "admin") == 0){
                                    GameRooms[RoomIndex].RoundsLimit = atoi(users[i].recv[1].c_str());
                                    printf("set round limit in room %s to %s\n", GameRooms[RoomIndex].RoomName.c_str(), users[i].recv[1].c_str());
                                }

                                if (strcmp(users[i].recv[0].c_str(), "LeaveRoom") == 0)
                                {
                                    int RoomIndex = findroom(users[i].CustomRoom);
                                    users[i].room = "Start";
                                    users[i].CustomRoom = "";
                                    users[i].InActiveGame = false;
                                    bool found = false;
                                    GameRooms[RoomIndex].NumberOfPlayers--;

                                    for (int j = 0; j < GameRooms[RoomIndex].NumberOfPlayers; j++)
                                    {
                                        if (i == GameRooms[RoomIndex].players[j])
                                        {
                                            found = true;
                                        }
                                        if (found)
                                            GameRooms[RoomIndex].players[j] = GameRooms[RoomIndex].players[j + 1];
                                    }
                                    if (GameRooms[RoomIndex].owner == i && GameRooms[RoomIndex].NumberOfPlayers > 0)
                                        GameRooms[RoomIndex].owner = GameRooms[RoomIndex].players[0];
                                    printf("left successfuly\n");
                                    std::string msg = "left successfuly\n";
                                    write(fds[i].fd, msg.c_str(), msg.size());
                                }

                                if (strcmp(users[i].recv[0].c_str(), "SendAnswers") == 0 && users[i].InActiveGame == true)
                                {
                                    printf("got something\n");
                                    int RoomIndex = findroom(users[i].CustomRoom);
                                    // std::chrono::_V2::system_clock::time_point CurrentTime = std::chrono::system_clock::now();
                                    if (response.size() <= 6)
                                    {
                                        for (int j = 1; j < response.size(); j++)
                                        {
                                            users[i].word[j - 1] = users[i].recv[j];
                                            if (!GameRooms[RoomIndex].EndGame)
                                            {
                                                users[i].TimePoints++;
                                                GameRooms[RoomIndex].EndGame = true;
                                                GameRooms[RoomIndex].StopTime = std::chrono::system_clock::now();

                                                // dosyłam info o odliczaniu dla klientów
                                                std::string msg = "RoundEnding 10\n";
                                                sendToAllInRoom(msg.c_str(), msg.size(), GameRooms[RoomIndex].RoomName);
                                                printf("Rozpoczeto odliczanie dla pokoju %s\n", GameRooms[RoomIndex].RoomName.c_str());
                                            }
                                        }
                                    }
                                    else
                                    {
                                        write(fds[i].fd, "bad answers\n", sizeof("bad answers\n"));
                                    }

                                    printf("user %s gave answers: \n", users[i].username.c_str());
                                    for (int j = 0; j < response.size() - 1; j++)
                                    {
                                        printf("%s\n", users[i].word[j].c_str());
                                    }
                                }
                            }
                            else if (strcmp(users[i].room.c_str(), "CustomRoom") == 0 && users[i].InActiveGame == false)
                            {
                                if (strcmp(users[i].recv[0].c_str(), "SetRoundLimit") == 0 && (GameRooms[findroom(users[i].CustomRoom)].owner == i || strcmp(users[i].username.c_str(), "admin") == 0)){
                                    int RoomIndex = findroom(users[i].CustomRoom);
                                    GameRooms[RoomIndex].RoundsLimit = atoi(users[i].recv[1].c_str());
                                    printf("set round limit in room %s to %s\n", GameRooms[RoomIndex].RoomName.c_str(), users[i].recv[1].c_str());
                                }else if (strcmp(users[i].recv[0].c_str(), "AddPlayerToRoom") == 0)
                                {

                                    if (response.size() == 2)
                                    {

                                        printf("Player added: %s\n", users[i].recv[1].c_str());
                                        write(fds[i].fd, "AddedPlayer\n", sizeof("AddedPlayer\n"));

                                        for (int j = 0; j < NumberOfRooms; j++)
                                        {
                                            if (strcmp(users[i].CustomRoom.c_str(), GameRooms[j].RoomName.c_str()) == 0)
                                            {
                                                printf("Room exists!\n");
                                                for (int k = 0; k < NumberOfUsers; k++)
                                                {
                                                    printf("%d\n", NumberOfUsers);
                                                    printf("searching for player... %s %s\n", users[i].recv[1].c_str(), users[k].username.c_str());
                                                    if (strcmp(users[i].recv[1].c_str(), users[k].username.c_str()) == 0)
                                                    {
                                                        GameRooms[j].players[GameRooms[j].NumberOfPlayers] = k;
                                                        GameRooms[j].NumberOfPlayers++;
                                                        users[k].room = "CustomRoom";
                                                        users[k].CustomRoom = GameRooms[k].RoomName;
                                                        printf("success!!!!\n");
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    else
                                    {
                                        printf("Incorrect command\n");
                                    }
                                }
                                else if (strcmp(users[i].recv[0].c_str(), "StartGame") == 0)
                                {
                                    int RoomIndex = findroom(users[i].CustomRoom);

                                    if (!GameRooms[RoomIndex].ActiveGame) {
                                        // resetuje licznik, jesli konczy sie gra
                                        if (GameRooms[RoomIndex].RoundsLeft <= 0)
                                            GameRooms[RoomIndex].RoundsLeft = GameRooms[RoomIndex].RoundsLimit;

                                        GameRooms[RoomIndex].RoundsLeft--;
                                        StartGame(i);
                                        GameRooms[RoomIndex].ContinueGameTimer = std::chrono::system_clock::now();
                                        GameRooms[RoomIndex].ContinueGame = true;
                                    }
                                }
                                else if (strcmp(users[i].recv[0].c_str(), "ChangeRoomName") == 0 && strcmp(users[i].username.c_str(), "admin") == 0)
                                {
                                    int RoomIndex = findroom(users[i].CustomRoom);
                                    GameRooms[RoomIndex].RoomName = users[i].recv[1];
                                    for (int j = 0; j < GameRooms[RoomIndex].NumberOfPlayers; j++)
                                    {
                                        users[GameRooms[RoomIndex].players[j]].CustomRoom = users[i].recv[1];
                                    }
                                    printf("changed room name\n");
                                }
                                else if (strcmp(users[i].recv[0].c_str(), "KickPlayer") == 0)
                                {
                                    int RoomIndex = findroom(users[i].CustomRoom);
                                    if ((GameRooms[RoomIndex].owner == i || strcmp(users[i].username.c_str(), "admin") == 0) && strcmp(users[i].username.c_str(), users[i].recv[1].c_str()) != 0)
                                    {
                                        bool found = false;
                                        for (int j = 0; j < GameRooms[RoomIndex].NumberOfPlayers; j++)
                                        {
                                            if (strcmp(users[GameRooms[RoomIndex].players[j]].username.c_str(), users[i].recv[1].c_str()) == 0)
                                            {
                                                users[GameRooms[RoomIndex].players[j]].room = "Start";
                                                users[GameRooms[RoomIndex].players[j]].CustomRoom = "";
                                                if (GameRooms[RoomIndex].owner == GameRooms[RoomIndex].players[j])
                                                    GameRooms[RoomIndex].owner = i;
                                                found = true;
                                                GameRooms[RoomIndex].NumberOfPlayers--;
                                                printf("kicked player %s\n", users[i].recv[1].c_str());
                                            }

                                            if (found && j + 1 < GameRooms[RoomIndex].NumberOfPlayers + 1)
                                            {
                                                GameRooms[RoomIndex].players[j] = GameRooms[RoomIndex].players[j + 1];
                                            }
                                        }
                                    }
                                    else
                                    {
                                        printf("tried to kick player without perms\n");
                                    }
                                }
                                else if (strcmp(users[i].recv[0].c_str(), "LeaveRoom") == 0)
                                {
                                    int RoomIndex = findroom(users[i].CustomRoom);
                                    users[i].room = "Start";
                                    users[i].CustomRoom = "";
                                    users[i].InActiveGame = false;
                                    bool found = false;
                                    GameRooms[RoomIndex].NumberOfPlayers--;

                                    for (int j = 0; j < GameRooms[RoomIndex].NumberOfPlayers; j++)
                                    {
                                        if (i == GameRooms[RoomIndex].players[j])
                                        {
                                            found = true;
                                        }
                                        if (found)
                                            GameRooms[RoomIndex].players[j] = GameRooms[RoomIndex].players[j + 1];
                                    }
                                    if (GameRooms[RoomIndex].owner == i && GameRooms[RoomIndex].NumberOfPlayers > 0)
                                        GameRooms[RoomIndex].owner = GameRooms[RoomIndex].players[0];
                                    printf("left successfuly\n");
                                    std::string msg = "left successfuly\n";
                                    write(fds[i].fd, msg.c_str(), msg.size());
                                }
                                else if (strcmp(users[i].recv[0].c_str(), "DeleteRoom") == 0)
                                {
                                    int RoomIndex = findroom(users[i].CustomRoom);
                                    if (GameRooms[RoomIndex].owner == i || strcmp(users[i].username.c_str(), "admin") == 0)
                                    {
                                        for (int j = 0; j < GameRooms[RoomIndex].NumberOfPlayers; j++)
                                        {
                                            users[GameRooms[RoomIndex].players[j]].room = "Start";
                                            users[GameRooms[RoomIndex].players[j]].CustomRoom = "";
                                        }
                                        for (int j = RoomIndex; j < NumberOfRooms; j++)
                                        {
                                            GameRooms[j] = GameRooms[j + 1];
                                        }
                                        NumberOfRooms--;
                                        printf("deleted given room\n");
                                    }
                                }
                            }

                            else
                            {
                                responses.insert({fds[i].fd, response});
                            }
                        }
                    }
                }
            }

            if (disconnect)
            {
                printf("Rozłączanie klienta numer %d\n", fds[i].fd);

                shutdown(fds[i].fd, SHUT_RDWR);
                close(fds[i].fd);

                for (int j = i; j < fdCount; j++)
                {
                    fds[j] = fds[j + 1];
                    users[j] = users[j + 1];
                }

                fdCount--;
                i--;
            }
        }

        for (int i = 1; i < NumberOfRooms; i++)
        {
            if (GameRooms[i].NumberOfPlayers >= 3 && GameRooms[i].ActiveGame == false)
            {
                if (GameRooms[i].CountingDownToStart == false)
                {
                    GameRooms[i].ThreePlayersEnteredTime = std::chrono::system_clock::now();
                    GameRooms[i].CountingDownToStart = true;
                }
                else if (GameRooms[i].GameStartLimit < (std::chrono::system_clock::now() - GameRooms[i].ThreePlayersEnteredTime).count() / 1000000)
                {
                    GameRooms[i].ContinueGame = true;
                    GameRooms[i].RoundsLeft = GameRooms[i].RoundsLimit;
                }
            }
            else
            {
                GameRooms[i].CountingDownToStart = false;
            }
            if (GameRooms[i].RoundsLeft > 0)
            {
                GameRooms[i].ContinueGame = true;
                if (GameRooms[i].ContinueGameTimeLimit < (std::chrono::system_clock::now() - GameRooms[i].ContinueGameTimer).count() / 1000000 && GameRooms[i].StartAgain == true && GameRooms[i].ActiveGame == false)
                {
                    StartGame(GameRooms[i].owner);
                    GameRooms[i].RoundsLeft--;
                }
            }
            else
            {
                GameRooms[i].ContinueGame = false;
            }
            if (GameRooms[i].ActiveGame)
            {

                if (GameRooms[i].TimeLimit < (int)((std::chrono::system_clock::now() - GameRooms[i].StartTime).count() / 1000000) || (GameRooms[i].EndGame == true && GameRooms[i].StopLimit < (int)((std::chrono::system_clock::now() - GameRooms[i].StopTime).count() / 1000000)))
                {
                    if (GameRooms[i].NumberOfPlayers < 3)
                        GameRooms[i].RoundsLeft = 0;
                    printf("%d\n", GameRooms[i].NumberOfPlayers);
                    int MaxPoints = 0;
                    std::vector<std::string> winners;
                    for (int j = 0; j < GameRooms[i].NumberOfPlayers; j++)
                    {
                        users[GameRooms[i].players[j]].InActiveGame = false;
                        for (int k = 0; k < 5; k++)
                        {
                            if (users[GameRooms[i].players[j]].word[k][0] == GameRooms[i].GameLetter || users[GameRooms[i].players[j]].word[k][0] == GameRooms[i].GameLetter + 32)
                            {
                                bool p20 = true;
                                for (int l = 0; l < GameRooms[i].NumberOfPlayers; l++)
                                {
                                    if (l != GameRooms[i].players[j])
                                    {
                                        if (strcmp(users[GameRooms[i].players[j]].word[k].c_str(), users[l].word[k].c_str()) == 0)
                                        {
                                            p20 = false;
                                            users[GameRooms[i].players[j]].points += 10;
                                            break;
                                        }
                                    }
                                }
                                if (p20)
                                {
                                    users[GameRooms[i].players[j]].points += 20;
                                }
                            }
                        }
                        printf("player %s got %d points\n", users[GameRooms[i].players[j]].username.c_str(), users[GameRooms[i].players[j]].points);

                        std::string msg = "Points:" + users[GameRooms[i].players[j]].username + ":" + std::to_string(users[GameRooms[i].players[j]].points) + "\n";
                        sendToAllInRoom(msg.c_str(), msg.size(), GameRooms[i].RoomName);

                        if (users[GameRooms[i].players[j]].points > MaxPoints)
                        {
                            MaxPoints = users[GameRooms[i].players[j]].points;
                            winners.clear();
                            winners.push_back(users[GameRooms[i].players[j]].username);
                        }
                        else if (users[GameRooms[i].players[j]].points == MaxPoints)
                        {
                            winners.push_back(users[GameRooms[i].players[j]].username);
                        }
                    }

                    if (winners.size() > 1)
                    {
                        printf("winners of round:\n");
                        sendToAllInRoom("winners of round:\n", sizeof("winners of round:\n"), GameRooms[i].RoomName);
                    }
                    else
                    {
                        printf("winner of round:\n");
                        sendToAllInRoom("winner of round:\n", sizeof("winner of round:\n"), GameRooms[i].RoomName);
                    }
                    for (int k = 0; k < winners.size(); k++)
                    {
                        printf("%s\n", winners[k].c_str());
                        std::string msg = winners[k] + '\n';
                        sendToAllInRoom(msg.c_str(), msg.size(), GameRooms[i].RoomName);
                    }
                    GameRooms[i].ActiveGame = false;
                    GameRooms[i].EndGame = false;
                    for (int j = 0; j < GameRooms[i].NumberOfPlayers; j++)
                    {
                        users[GameRooms[i].players[j]].GamePoints += users[GameRooms[i].players[j]].points;
                        users[GameRooms[i].players[j]].points = 0;
                        for (int k = 0; k < 5; k++)
                        {
                            users[GameRooms[i].players[j]].word[k] = "";
                        }
                    }
                    int MaxEndPoints = 0;
                    winners.clear();
                    if (GameRooms[i].RoundsLeft <= 0)
                    {
                        for (int j = 0; j < GameRooms[i].NumberOfPlayers; j++)
                        {
                            if (users[GameRooms[i].players[j]].GamePoints + users[GameRooms[i].players[j]].TimePoints > MaxEndPoints)
                            {
                                MaxEndPoints = users[GameRooms[i].players[j]].GamePoints + users[GameRooms[i].players[j]].TimePoints;
                                winners.clear();
                                winners.push_back(users[GameRooms[i].players[j]].username);
                            }
                            else if (users[GameRooms[i].players[j]].points == MaxEndPoints)
                            {
                                winners.push_back(users[GameRooms[i].players[j]].username);
                            }
                            users[GameRooms[i].players[j]].GamePoints = 0;
                            users[GameRooms[i].players[j]].TimePoints = 0;
                        }
                        if (winners.size() > 1)
                        {
                            printf("winners:\n");
                            sendToAllInRoom("winners:\n", sizeof("winners:\n"), GameRooms[i].RoomName);
                        }
                        else
                        {
                            printf("winner:\n");
                            sendToAllInRoom("winner:\n", sizeof("winner:\n"), GameRooms[i].RoomName);
                        }
                        printf("They got %d points\n",MaxEndPoints);
                        for (int k = 0; k < winners.size(); k++)
                        {
                            printf("%s\n", winners[k].c_str());
                            std::string msg = winners[k] + '\n';
                            sendToAllInRoom(msg.c_str(), msg.size(), GameRooms[i].RoomName);
                        }
                        for (int j = 0; j < GameRooms[i].NumberOfPlayers; j++)
                        {
                            users[GameRooms[i].players[j]].room = "Start";
                            users[GameRooms[i].players[j]].CustomRoom = "";
                            users[GameRooms[i].players[j]].InActiveGame = false;

                            std::string msg = "left successfuly\n";
                            write(fds[GameRooms[i].players[j]].fd, msg.c_str(), msg.size());
                        }
                        GameRooms[i].NumberOfPlayers = 0;
                    }
                    else
                    {
                        GameRooms[i].StartAgain = true;
                    }

                    GameRooms[i].ContinueGameTimer = std::chrono::system_clock::now();
                }
            }
        }

        sendRoomInformationInLobby();

        sendRoomInformation();
    }

    close(servSock);
}
