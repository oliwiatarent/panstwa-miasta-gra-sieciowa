#include <QApplication>
#include <QPushButton>
#include <QMessageBox>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <string>

void onButtonClicked(char* ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr = {};
    serverAddr.sin_addr.s_addr = inet_addr(ip);;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    if (connect(sock, (sockaddr*) &serverAddr, sizeof(serverAddr)) == -1) {
        perror("Connect failed");
        close(sock);
        return;
    }

    char buf[255];

    write(sock, "Request\n", 8);
    int bytes = read(sock, buf, 255);

    shutdown(sock, SHUT_RDWR);
    close(sock);
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    char* ip = argv[1];
    int port = atoi(argv[2]);

    QPushButton button("Kliknij mnie!");
    button.resize(200, 50);

    QObject::connect(&button, &QPushButton::clicked, [=]() { onButtonClicked(ip, port); });

    button.show();
    return app.exec();
}
