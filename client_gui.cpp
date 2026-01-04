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
#include <QListWidget>
#include <QTableWidget>
#include <QSpinBox>
#include <QHeaderView>


class NetworkWorker : public QObject {
    Q_OBJECT

signals:
    void logMessage(QString msg);
    void connectedToServer();
    void loginSuccess();
    void loginFailed(QString msg);
    void roundEndingSoon();
    void roundFinished();
    void roomListItem(QString name, QString players, QString status);
    void roomListCleared();
    void scoreUpdate(QString user, int points);
    void roomJoin();
    void roomLeft();
    void playerLeftGame(QString player);
    void gameStarted(QString letter);

public:
    int sock = -1;
    std::atomic<bool> running;
    bool logged_in = false;
    char username[255];
    QString inputBuff;  // do buforowania reada

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
                        inputBuff += QString::fromUtf8(buf);
                        //QString buff = QString::fromUtf8(buf);
                        //QStringList lines = buff.split('\n', Qt::SkipEmptyParts);
                        //emit logMessage(msg);

                        while (inputBuff.contains('\n')) {
                            int lineEnd = inputBuff.indexOf('\n');
                            QString msg = inputBuff.left(lineEnd).trimmed();
                            inputBuff.remove(0, lineEnd + 1);
                            if (msg.isEmpty())
                                continue;

                            // lista pokoi
                            if (msg == "ROOMLIST_START") {
                                emit roomListCleared();
                            }
                            else if (msg.contains("ROOM:")) {
                                // ROOM:Nazwa:LGraczy:Status
                                QStringList parts = msg.split(":");
                                emit roomListItem(parts[1], parts[2], parts[3]);
                            }
                            // tabela
                            else if (msg.contains("Points:")) {
                                // Points:User:10
                                QStringList parts = msg.split(":");
                                emit scoreUpdate(parts[1], parts[2].toInt());
                            }
                            // wchodzenie/wychodzenie
                            else if (msg.contains("Joining Room") || msg.contains("New Room Created")) {
                                emit roomJoin();
                            }
                            else if (msg.contains("left successfuly")) {
                                emit roomLeft();
                            }
                            else if (msg.contains("Litera:")) {
                                // Litera: X
                                QString letter = msg.section(":", 1).trimmed();
                                emit gameStarted(letter);
                            }
                            else if (msg.contains("PlayerLeft")) {
                                // PlayerLeft:nick
                                QString player = msg.section(":", 1).trimmed();
                                emit playerLeftGame(player);
                            }
                            else {
                                emit logMessage(msg);

                                //sygnal do odliczania
                                if (msg.contains("RoundEnding")) {
                                    emit roundEndingSoon();
                                }

                                //sygnal konca rundy
                                if (msg.contains("winner"))
                                    emit roundFinished();

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

    // Ekran 2 - lobby
    QListWidget *listRooms;
    QLineEdit *inputNewRoomName;
    QSpinBox *cntRounds;
    QSpinBox *cntPlayers;
    QPushButton *btnCreate, *btnJoin;
    QString lastSelectedRoom = "";

    // Ekran 3 - gra
    QLabel *currRoom;
    QTableWidget *tableScores;
    QLineEdit *ans1, *ans2, *ans3, *ans4, *ans5;
    QPushButton *btnStart, *btnSendAnswers, *btnLeave;
    QLabel *notif;
    QString currRoomName;
    QLabel *letter;


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
            appendLog("[INFO] Logowanie nie powiodlo sie!");
            btnConnect->setEnabled(true);
            appendLog(msg);
            if (netThread.joinable())
                netThread.join();
        });

        connect(worker, &NetworkWorker::roomListCleared, this, [this](){
            if (stackedWidget->currentIndex() == 1) {
                QListWidgetItem *currentItem = listRooms->currentItem();
                if (currentItem)
                    lastSelectedRoom = currentItem->data(Qt::UserRole).toString();
                else
                    lastSelectedRoom = "";
                listRooms->clear();
            }
        });

        connect(worker, &NetworkWorker::roomListItem, this, [this](QString name, QString players, QString status){
            if(stackedWidget->currentIndex() == 1) {
                QString label = QString("%1 | %2 | %3").arg(name, players, status);
                QListWidgetItem* item = new QListWidgetItem(label);
                // tylko nazwa pokoju
                item->setData(Qt::UserRole, name);
                listRooms->addItem(item);

                if (name == lastSelectedRoom) {
                    item->setSelected(true);
                    listRooms->setCurrentItem(item);
                }
            }
        });

        connect(worker, &NetworkWorker::roomJoin, this, [this](){
            stackedWidget->setCurrentIndex(2); // ekran gry
            tableScores->setRowCount(0);
            currRoom->setText("Pokoj: " + currRoomName);
            appendLog("[INFO] Dolaczono do pokoju.");
        });

        connect(worker, &NetworkWorker::roomLeft, this, [this](){
            stackedWidget->setCurrentIndex(1); // ekran lobby
            currRoomName = "";
            appendLog("[INFO] Opuszczono pokoj.");
        });

        connect(worker, &NetworkWorker::scoreUpdate, this, [this](QString user, int points){
            updateScoreboard(user, points);
        });

        connect(worker, &NetworkWorker::roundEndingSoon, this, [this](){
            notif->setText("Uwaga! Runda zakonczy sie za chwile!");
            notif->setVisible(true);
        });

        connect(worker, &NetworkWorker::roundFinished, this, [this](){
            notif->setVisible(false);
            appendLog("[INFO] Koniec rundy. Wyniki:");
        });

        connect(worker, &NetworkWorker::gameStarted, this, [this](QString lett){
            letter->setText("Litera: " + lett);
            appendLog("[INFO] Rozpoczeto gre. Litera: " + lett);
            notif->setVisible(false);
        });

        connect(worker, &NetworkWorker::playerLeftGame, this, [this](QString player){
            markPlayerAsLeft(player);
            appendLog("[INFO] Gracz " + player + " opuscil gre.");
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
        QString name = inputNewRoomName->text().trimmed();
        if (name.isEmpty())
            return;
        currRoomName = name;

        worker->sendCommand("CreateNewRoom " + name.toStdString());
        QString rounds = "SetRoundLimit " + QString::number(cntRounds->value());
        worker->sendCommand(rounds.toStdString());
        QString players = "SetPlayersLimit " + QString::number(cntPlayers->value());
        worker->sendCommand(players.toStdString());
    }

    void onJoinRoom() {
        QListWidgetItem *item = listRooms->currentItem();
        if (!item) {
            appendLog("[WARN] Zaznacz pokoj na liscie!");
            return;
        }
        QString roomName = item->data(Qt::UserRole).toString();
        currRoomName = roomName;
        worker->sendCommand("JoinRoom " + roomName.toStdString());
    }

    void onLeaveRoom() {
        worker->sendCommand("LeaveRoom");
        ans1->clear();
        ans2->clear();
        ans3->clear();
        ans4->clear();
        ans5->clear();
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

    void updateScoreboard(QString user, int points) {
        for (int i=0; i<tableScores->rowCount(); ++i) {
            if (tableScores->item(i, 0)->text() == user) {
                tableScores->item(i, 1)->setText(QString::number(points));
                for (int col=0; col<2; ++col) {
                    QFont f = tableScores->item(i, col)->font();
                    f.setStrikeOut(false);
                    tableScores->item(i, col)->setFont(f);
                }
                return;
            }
        }
        // dodaje nowego gracza do tabeli
        int row = tableScores->rowCount();
        tableScores->insertRow(row);
        tableScores->setItem(row, 0, new QTableWidgetItem(user));
        tableScores->setItem(row, 1, new QTableWidgetItem(QString::number(points)));
    }

    void markPlayerAsLeft(QString player) {
        for(int i = 0; i < tableScores->rowCount(); ++i) {
            if(tableScores->item(i, 0)->text() == player) {
                for(int col = 0; col < 2; ++col) {
                    QTableWidgetItem* item = tableScores->item(i, col);
                    QFont font = item->font();
                    font.setStrikeOut(true);
                    item->setFont(font);
                }
                break;
            }
        }
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
        btnConnect = new QPushButton("Polacz!");
        connect(btnConnect, &QPushButton::clicked, this, &MainWindow::onConnect);

        layoutLogin->addWidget(new QLabel("IP Serwera:")); layoutLogin->addWidget(inputIp);
        layoutLogin->addWidget(new QLabel("Port:")); layoutLogin->addWidget(inputPort);
        layoutLogin->addWidget(new QLabel("Nick:")); layoutLogin->addWidget(inputNick);
        layoutLogin->addWidget(btnConnect);
        layoutLogin->addStretch();

        // lobby
        QWidget *pageLobby = new QWidget;
        QVBoxLayout *layoutLobby = new QVBoxLayout(pageLobby);

        // pokoje:
        // tworzenie pokoju
        QGroupBox *createRoom = new QGroupBox("Stworz Pokoj!");
        QHBoxLayout *layoutCreate = new QHBoxLayout(createRoom);
        inputNewRoomName = new QLineEdit(); inputNewRoomName->setPlaceholderText("Nazwa pokoju");
        cntRounds = new QSpinBox();
        cntRounds->setRange(1, 20);
        cntRounds->setValue(2);
        cntRounds->setPrefix("Rundy: ");
        cntPlayers = new QSpinBox();
        cntPlayers->setRange(2, 8);
        cntPlayers->setValue(4);
        cntPlayers->setPrefix("Gracze: ");
        btnCreate = new QPushButton("Stworz");
        connect(btnCreate, &QPushButton::clicked, this, &MainWindow::onCreateRoom);
        layoutCreate->addWidget(inputNewRoomName);
        layoutCreate->addWidget(cntRounds);
        layoutCreate->addWidget(cntPlayers);
        layoutCreate->addWidget(btnCreate);

        // lista
        QGroupBox *roomList = new QGroupBox("Dostepne Pokoje:");
        QVBoxLayout *layoutList = new QVBoxLayout(roomList);
        listRooms = new QListWidget();
        btnJoin = new QPushButton("Dolacz do zaznaczonego!");
        connect(btnJoin, &QPushButton::clicked, this, &MainWindow::onJoinRoom);
        layoutList->addWidget(listRooms);
        layoutList->addWidget(btnJoin);

        layoutLobby->addWidget(createRoom);
        layoutLobby->addWidget(roomList);

        // gra:
        QWidget *pageRoom = new QWidget;
        QVBoxLayout *layoutRoom = new QVBoxLayout(pageRoom);

        // pokoj
        QHBoxLayout *topBar = new QHBoxLayout;
        currRoom = new QLabel("Pokoj:");
        currRoom->setStyleSheet("font-weight: bold;");
        letter = new QLabel("Litera: ?");
        letter->setStyleSheet("font-weight: bold; color: green;");
        btnLeave = new QPushButton("Wyjdz z pokoju");
        btnLeave->setStyleSheet("background-color: red; color: white;");
        connect(btnLeave, &QPushButton::clicked, this, &MainWindow::onLeaveRoom);
        topBar->addWidget(currRoom);
        topBar->addWidget(letter);
        topBar->addStretch();
        topBar->addWidget(btnLeave);

        // tabela
        tableScores = new QTableWidget(0, 2);
        tableScores->setHorizontalHeaderLabels({"Gracz", "Punkty"});
        tableScores->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        tableScores->setEditTriggers(QAbstractItemView::NoEditTriggers);
        tableScores->setMaximumHeight(300);

        // odpowiedzi
        QGroupBox *grpAns = new QGroupBox("Twoje Odpowiedzi:");
        QVBoxLayout *boxAns = new QVBoxLayout(grpAns);
        ans1 = new QLineEdit(); ans1->setPlaceholderText("Panstwo");
        ans2 = new QLineEdit(); ans2->setPlaceholderText("Miasto");
        ans3 = new QLineEdit(); ans3->setPlaceholderText("Rzeka");
        ans4 = new QLineEdit(); ans4->setPlaceholderText("Potrawa");
        ans5 = new QLineEdit(); ans5->setPlaceholderText("Imie");
        btnSendAnswers = new QPushButton("Wyslij Odpowiedzi!");
        connect(btnSendAnswers, &QPushButton::clicked, this, &MainWindow::onSendAnswers);

        boxAns->addWidget(ans1); boxAns->addWidget(ans2); boxAns->addWidget(ans3);
        boxAns->addWidget(ans4); boxAns->addWidget(ans5); boxAns->addWidget(btnSendAnswers);

        btnStart = new QPushButton("Start rundy!");
        btnStart->setStyleSheet("background-color: green; color: white; padding: 10px;");
        connect(btnStart, &QPushButton::clicked, this, &MainWindow::onStartGame);

        notif = new QLabel("");
        notif->setStyleSheet("background-color: red; color: white; padding: 10px;");
        notif->setAlignment(Qt::AlignCenter);
        notif->setVisible(false);

        layoutRoom->addLayout(topBar);
        layoutRoom->addWidget(new QLabel("Wyniki:"));
        layoutRoom->addWidget(tableScores);
        layoutRoom->addWidget(grpAns);
        layoutRoom->addWidget(notif);
        layoutRoom->addWidget(btnStart);

        // strony na stacku
        stackedWidget->addWidget(pageLogin); //0
        stackedWidget->addWidget(pageLobby); //1
        stackedWidget->addWidget(pageRoom);  //2

        // [TESTOWO] konsola log�w
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
    w.resize(500, 700);
    w.show();
    return app.exec();
}
