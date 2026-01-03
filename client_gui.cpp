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
#include <thread>
#include <atomic>

// includy dla Qt
#include <QApplication>
#include <QMainWindow>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTextEdit>
#include <QStackedWidget>
#include <QMessageBox>
#include <QThread>
#include <QMetaType>
#include <QGroupBox>


class NetworkWorker : public QObject {
    Q_OBJECT

signals:
    void logMessage(QString msg);
    void connectedToServer();
    void loginSuccess();
    void loginFailed(QString msg);

public:
    int sock = -1;
    std::atomic<bool> running;
    bool logged_in = false;
    char username[255];

    NetworkWorker() { running = true; }

    // wysylanie danych
    void sendCommand(std::string cmd) {
        if (sock != -1 && running) {
            // dodaje \n dla konca danych
            if (cmd.back() != '\n') cmd += "\n";
            write(sock, cmd.c_str(), cmd.size()); //do zmiany?
        }
    }

    // main
    void runNetworkLoop(std::string ip, int port) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock == -1) {
            emit logMessage("[ERROR] Nie mozna utworzyc socketu.");
            return;
        }

        sockaddr_in serverAddr = {};
        serverAddr.sin_addr.s_addr = inet_addr(ip.c_str());
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);

        if (::connect(sock, (sockaddr*) &serverAddr, sizeof(serverAddr)) == -1) {
            emit logMessage("[ERROR] Nie mozna sie polaczyc.");
            close(sock);
            return;
        }

        pollfd fds[1];
        fds[0].fd = sock;
        fds[0].events = POLLIN | POLLHUP | POLLERR;
        emit connectedToServer();

        while (running) {
            int ready = poll(fds, 1, 100); // 100ms

            if (ready > 0) {
                if (fds[0].revents & (POLLHUP | POLLERR)) {
                    emit logMessage("[INFO] Rozlaczono z serwerem.");
                    break;
                }

                if (fds[0].revents & POLLIN) {
                    char buf[256]{};
                    int bytes = read(sock, buf, 255);

                    if (bytes > 0) {
                        buf[bytes] = '\0';
                        QString msg = QString::fromUtf8(buf).trimmed();
                        emit logMessage(msg);

                        //logowanie
                        if (!logged_in && msg.contains("Username available")) {
                            logged_in = true;
                            emit loginSuccess();
                        }
                        else if (msg.contains("Username already in use")) {
                            emit loginFailed("[WARN] Nazwa uzytkownika jest zajeta!");
                            running = false;
                        }
                    }
                }
            }
        }
        close(sock);
    }
};

class MainWindow : public QMainWindow {
    Q_OBJECT

    NetworkWorker* worker;
    std::thread netThread;

    // UI
    QStackedWidget *stackedWidget;
    QTextEdit *consoleLog;

    // Ekran 1 - login
    QLineEdit *inputIp, *inputPort, *inputNick;
    QPushButton *btnConnect;

    // Ekran 2 - lobby/gra
    QLineEdit *inputCmd;
    QPushButton *btnCreate, *btnJoin, *btnSendRaw;
    QLineEdit *ans1, *ans2, *ans3, *ans4, *ans5;
    QPushButton *btnSendAnswers;

public:
    MainWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
        worker = new NetworkWorker();
        setupUI();
        qRegisterMetaType<QString>("QString");

        connect(worker, &NetworkWorker::logMessage, this, &MainWindow::appendLog, Qt::QueuedConnection);

        connect(worker, &NetworkWorker::connectedToServer, this, [this](){
            appendLog("[INFO] Polaczono! Wysylam nick...");
            worker->sendCommand(inputNick->text().toStdString());
        });

        connect(worker, &NetworkWorker::loginSuccess, this, [this](){
            appendLog("[INFO] Zalogowano pomyslnie!");
            stackedWidget->setCurrentIndex(1); // zmiana ekranu
        });

        connect(worker, &NetworkWorker::loginFailed, this, [this](QString msg){
            btnConnect->setEnabled(true);
            if (netThread.joinable())
                netThread.join();
        });
    }

    ~MainWindow() {
        worker->running = false;
        if (netThread.joinable())
            netThread.join();
        delete worker;
    }

private slots:
    void appendLog(QString msg) {
        consoleLog->append(msg);
    }

    void onConnect() {
        btnConnect->setEnabled(false);
        if (netThread.joinable())
            netThread.join();
        worker->running = true;

        std::string ip = inputIp->text().toStdString();
        int port = inputPort->text().toInt();

        netThread = std::thread([this, ip, port](){
            worker->runNetworkLoop(ip, port);
        });
    }

    void onCreateRoom() {
        QString cmd = "CreateNewRoom " + inputCmd->text();
        worker->sendCommand(cmd.toStdString());
    }

    void onJoinRoom() {
        QString cmd = "JoinRoom " + inputCmd->text();
        worker->sendCommand(cmd.toStdString());
    }

    void onStartGame() {
        worker->sendCommand("StartGame");
    }

    void onSendAnswers() {
        // przetworzenie tekstu
        auto sanitize = [](QString text) {
            text = text.trimmed();
            if(text.isEmpty()) return QString("-"); // zeby nie odsylal pustej wartosci
            return text.replace(" ", "_"); // dla 2+ slow
        };

        QString msg = "SendAnswers " +
                      sanitize(ans1->text()) + " " + sanitize(ans2->text()) + " " +
                      sanitize(ans3->text()) + " " + sanitize(ans4->text()) + " " +
                      sanitize(ans5->text());

        worker->sendCommand(msg.toStdString());
        appendLog("[INFO] Wyslano odpowiedzi.");
    }

    void setupUI() {
        QWidget *central = new QWidget;
        setCentralWidget(central);
        QVBoxLayout *mainLayout = new QVBoxLayout(central);

        stackedWidget = new QStackedWidget;

        // login
        QWidget *pageLogin = new QWidget;
        QVBoxLayout *layoutLogin = new QVBoxLayout(pageLogin);
        inputIp = new QLineEdit("127.0.0.1");
        inputPort = new QLineEdit("1234");
        inputNick = new QLineEdit("gracz1");
        inputNick->setPlaceholderText("Twoj Nick");
        btnConnect = new QPushButton("Polacz i Zaloguj!");
        connect(btnConnect, &QPushButton::clicked, this, &MainWindow::onConnect);

        layoutLogin->addWidget(new QLabel("IP Serwera:")); layoutLogin->addWidget(inputIp);
        layoutLogin->addWidget(new QLabel("Port:")); layoutLogin->addWidget(inputPort);
        layoutLogin->addWidget(new QLabel("Nick:")); layoutLogin->addWidget(inputNick);
        layoutLogin->addWidget(btnConnect);
        layoutLogin->addStretch();

        // lobby
        QWidget *pageGame = new QWidget;
        QVBoxLayout *layoutGame = new QVBoxLayout(pageGame);

        // pokoje
        QHBoxLayout *roomLayout = new QHBoxLayout;
        inputCmd = new QLineEdit();
        inputCmd->setPlaceholderText("Nazwa pokoju...");
        btnCreate = new QPushButton("Stworz pokoj");
        btnJoin = new QPushButton("Dolacz!");
        QPushButton *btnStart = new QPushButton("Start Rundy!");

        connect(btnCreate, &QPushButton::clicked, this, &MainWindow::onCreateRoom);
        connect(btnJoin, &QPushButton::clicked, this, &MainWindow::onJoinRoom);
        connect(btnStart, &QPushButton::clicked, this, &MainWindow::onStartGame);

        roomLayout->addWidget(inputCmd);
        roomLayout->addWidget(btnCreate);
        roomLayout->addWidget(btnJoin);

        // odpowiedzi
        QGroupBox *grpAns = new QGroupBox("Odpowiedzi:");
        QVBoxLayout *boxAns = new QVBoxLayout(grpAns);
        ans1 = new QLineEdit(); ans1->setPlaceholderText("Panstwo");
        ans2 = new QLineEdit(); ans2->setPlaceholderText("Miasto");
        ans3 = new QLineEdit(); ans3->setPlaceholderText("Rzeka");
        ans4 = new QLineEdit(); ans4->setPlaceholderText("Potrawa");
        ans5 = new QLineEdit(); ans5->setPlaceholderText("Imie");
        btnSendAnswers = new QPushButton("Przeslij!");
        connect(btnSendAnswers, &QPushButton::clicked, this, &MainWindow::onSendAnswers);

        boxAns->addWidget(ans1); boxAns->addWidget(ans2); boxAns->addWidget(ans3);
        boxAns->addWidget(ans4); boxAns->addWidget(ans5); boxAns->addWidget(btnSendAnswers);

        layoutGame->addLayout(roomLayout);
        layoutGame->addWidget(btnStart);
        layoutGame->addWidget(grpAns);

        // strony na stacku
        stackedWidget->addWidget(pageLogin); //0
        stackedWidget->addWidget(pageGame);  //1

        // [TESTOWO] konsola logów
        consoleLog = new QTextEdit;
        consoleLog->setReadOnly(true);
        consoleLog->setStyleSheet("background-color: #333; color: #0f0;");

        mainLayout->addWidget(stackedWidget);
        mainLayout->addWidget(new QLabel("Komunikaty:"));
        mainLayout->addWidget(consoleLog);
    }
};

#include "client_gui.moc"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MainWindow w;
    w.setWindowTitle("Panstwa-Miasta");
    w.resize(500, 600);
    w.show();
    return app.exec();
}
