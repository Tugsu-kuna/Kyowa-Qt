#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Initialize Sockets
    tcpSocket = new QTcpSocket(this);
    udpSocket = new QUdpSocket(this);

    setupConnections();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupConnections()
{
    // TCP Signal Mapping
    connect(tcpSocket, &QTcpSocket::connected, this, &MainWindow::onTcpConnected);
    connect(tcpSocket, &QTcpSocket::disconnected, this, &MainWindow::onTcpDisconnected);
    connect(tcpSocket, &QTcpSocket::errorOccurred, this, &MainWindow::onTcpError);

    // UDP Signal Mapping
    connect(udpSocket, &QUdpSocket::readyRead, this, &MainWindow::onUdpDataReady);

    // UI Signal Mapping (Assuming names from UI section)
    connect(ui->btnConnect, &QPushButton::clicked, this, &MainWindow::onConnectButtonClicked);
    connect(ui->btnForward, &QPushButton::clicked, this, &MainWindow::onCommandButtonClicked);
    connect(ui->btnBackward, &QPushButton::clicked, this, &MainWindow::onCommandButtonClicked);
    connect(ui->btnLeft, &QPushButton::clicked, this, &MainWindow::onCommandButtonClicked);
    connect(ui->btnRight, &QPushButton::clicked, this, &MainWindow::onCommandButtonClicked);
    connect(ui->checkAutoMode, &QCheckBox::toggled, this, &MainWindow::onAutoModeToggled);
}

// --- Networking Stubs ---

void MainWindow::onConnectButtonClicked()
{
    if (tcpSocket->state() == QAbstractSocket::UnconnectedState) {
        QString ip = ui->editIpAddress->text();
        int port = ui->spinPort->value();
        tcpSocket->connectToHost(ip, port);
    } else {
        tcpSocket->disconnectFromHost();
    }
}

void MainWindow::onTcpConnected()
{
    ui->lblStatus->setText("Connected");
    qDebug() << "TCP: Connected to robot.";
}

void MainWindow::onTcpDisconnected()
{
    ui->lblStatus->setText("Disconnected");
    qDebug() << "TCP: Disconnected.";
}

void MainWindow::onTcpError(QAbstractSocket::SocketError socketError)
{
    ui->lblStatus->setText("Error: " + tcpSocket->errorString());
    qDebug() << "TCP Error:" << socketError;
}

void MainWindow::onUdpDataReady()
{
    // Future logic: Receive datagrams and assemble JPEG frames
    while (udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = udpSocket->receiveDatagram();
        // Process datagram.data() here
    }
}

// --- Control Stubs ---

void MainWindow::onCommandButtonClicked()
{
    // Logic to identify sender and send TCP command strings like "MOVE_FWD"
    if (tcpSocket->state() == QAbstractSocket::ConnectedState) {
        // tcpSocket->write("COMMAND_HERE");
    }
}

void MainWindow::onAutoModeToggled(bool checked)
{
    qDebug() << "Automatic Mode:" << (checked ? "ON" : "OFF");
    // Send mode change via TCP
}
