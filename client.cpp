#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
#include <cstring>

int main(int argc, char *argv[]) {

    std::string CurrentRoom="Start";
    bool gameover=false;
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

    char buf[255];
    char username[20];

    strcpy(buf,"Username already in use");
    while(strcmp(buf,"Username already in use")==0){
        scanf("%s", username);
        write(sock, username, sizeof(username));
        read(sock,buf,sizeof(buf));
        printf("%s\n",buf);
    }
    char sendbuf[255];
    char recvbuf[255];
    std::string recv;
    while(!gameover){
        if(CurrentRoom.compare("Start")==0){
            printf("Type the action you want to take (CreateNewRoom for new room)\n");
            scanf("%s", sendbuf);
            write(sock, sendbuf, sizeof(sendbuf));
            read(sock,recvbuf,sizeof(recvbuf));
            printf("answer from server about new room creating %s\n",recvbuf);
            recv.assign(recvbuf,sizeof(recvbuf));
            if(recv.compare("NewRoomCreated")==0){
                write(sock,sendbuf,sizeof(sendbuf));
                read(sock,recvbuf,sizeof(recvbuf));
                printf("answear about name to the new room %s\n",recvbuf);
                recv.assign(recvbuf,sizeof(recvbuf));
                if(recv.compare("Good")){
                    CurrentRoom="CustomRoom";
                }
            }
        }else if(CurrentRoom.compare("CustomRoom")==0){
            printf("udalo sie\n");
        }
    }

    shutdown(sock, SHUT_RDWR);
    close(sock);
}
