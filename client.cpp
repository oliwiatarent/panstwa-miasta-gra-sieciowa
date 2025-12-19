#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>
#include <cstring>

int main(int argc, char *argv[]) {

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
        scanf("%s", &username);
        write(sock, username, sizeof(username));
        read(sock,buf,sizeof(buf));
        printf("%s\n",buf);
    }

    write(sock, "Request\n", 8);
    int bytes = read(sock, buf, 255);

    shutdown(sock, SHUT_RDWR);
    close(sock);
}
