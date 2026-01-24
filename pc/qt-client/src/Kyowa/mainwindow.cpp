#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QNetworkDatagram>

// Configuration
const int HEARTBEAT_INTERVAL_MS = 100; // Send cmds at 10Hz to satisfy Python 300ms Watchdog
const double SPEED_FWD = 0.5;
const double SPEED_TURN = 0.3;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    tcpSocket = new QTcpSocket(this);
    udpSocket = new QUdpSocket(this);

    // Initialize Heartbeat Timer
    heartbeatTimer = new QTimer(this);
    heartbeatTimer->setInterval(HEARTBEAT_INTERVAL_MS);

    setupConnections();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupConnections()
{
    // TCP Signals
    connect(tcpSocket, &QTcpSocket::connected, this, &MainWindow::onTcpConnected);
    connect(tcpSocket, &QTcpSocket::disconnected, this, &MainWindow::onTcpDisconnected);
    // Note: errorOccurred is for Qt 5.15+, use error() for older versions
    connect(tcpSocket, &QTcpSocket::errorOccurred, this, &MainWindow::onTcpError);

    // UDP Signals
    connect(udpSocket, &QUdpSocket::readyRead, this, &MainWindow::onUdpDataReady);

    // UI - Main Connect Button
    connect(ui->btnConnect, &QPushButton::clicked, this, &MainWindow::onConnectButtonClicked);
    connect(ui->checkAutoMode, &QCheckBox::toggled, this, &MainWindow::onAutoModeToggled);

    // UI - Movement Buttons (Using PRESSED/RELEASED for safety)
    // Connect all directional buttons to the generic handler
    QList<QPushButton*> buttons = {ui->btnForward, ui->btnBackward, ui->btnLeft, ui->btnRight};
    for(auto btn : buttons) {
        connect(btn, &QPushButton::pressed, this, &MainWindow::onMovePressed);
        connect(btn, &QPushButton::released, this, &MainWindow::onMoveReleased);
    }

    // Connect Heartbeat Timer to the sender function
    connect(heartbeatTimer, &QTimer::timeout, this, &MainWindow::sendCurrentCommand);
}

// --- Connection Logic ---

void MainWindow::onConnectButtonClicked()
{
    if (tcpSocket->state() == QAbstractSocket::UnconnectedState) {
        QString ip = ui->editIpAddress->text();
        int port = ui->spinPort->value();
        qDebug() << "Connecting to" << ip << ":" << port;
        tcpSocket->connectToHost(ip, port);
    } else {
        tcpSocket->disconnectFromHost();
    }
}

void MainWindow::onTcpConnected()
{
    ui->lblStatus->setText("Connected");
    ui->btnConnect->setText("Disconnect");
    qDebug() << "TCP: Connected.";
    // Start the heartbeat loop immediately
    heartbeatTimer->start();
}

void MainWindow::onTcpDisconnected()
{
    ui->lblStatus->setText("Disconnected");
    ui->btnConnect->setText("Connect");
    qDebug() << "TCP: Disconnected.";
    // Stop sending commands
    heartbeatTimer->stop();
}

void MainWindow::onTcpError(QAbstractSocket::SocketError socketError)
{
    ui->lblStatus->setText("Error: " + tcpSocket->errorString());
    heartbeatTimer->stop();
}

// --- Control Logic ---

void MainWindow::onMovePressed()
{
    // Identify which button sent the signal
    QObject* senderObj = sender();

    if (senderObj == ui->btnForward) {
        setLocalCommand(SPEED_FWD, SPEED_FWD);
    }
    else if (senderObj == ui->btnBackward) {
        setLocalCommand(-SPEED_FWD, -SPEED_FWD);
    }
    else if (senderObj == ui->btnLeft) {
        // Rotate Left: Left motor back, Right motor fwd
        setLocalCommand(-SPEED_TURN, SPEED_TURN);
    }
    else if (senderObj == ui->btnRight) {
        // Rotate Right: Left motor fwd, Right motor back
        setLocalCommand(SPEED_TURN, -SPEED_TURN);
    }
}

void MainWindow::onMoveReleased()
{
    // When button is let go, stop immediately
    setLocalCommand(0.0, 0.0);
}

void MainWindow::setLocalCommand(double left, double right)
{
    targetLeft = left;
    targetRight = right;

    // Optional: Send immediately for responsiveness,
    // though the timer will pick it up in <100ms anyway.
    sendCurrentCommand();
}

// --- The Heartbeat (Sends commands to Python) ---

void MainWindow::sendCurrentCommand()
{
    if (tcpSocket->state() != QAbstractSocket::ConnectedState) return;

    QString command;

    if (targetLeft == 0.0 && targetRight == 0.0) {
        command = "STOP\n";
    } else {
        // Format: WHEELS <left> <right>\n
        // Using fixed precision to ensure Python parses correctly
        command = QString("WHEELS %1 %2\n")
                      .arg(targetLeft, 0, 'f', 2)
                      .arg(targetRight, 0, 'f', 2);
    }

    tcpSocket->write(command.toUtf8());
    tcpSocket->flush(); // Ensure it leaves the buffer immediately
}

// --- Keyboard Overrides (Improvement) ---

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if(event->isAutoRepeat()) return; // Ignore hold-down repeats

    switch(event->key()) {
    case Qt::Key_W: setLocalCommand(SPEED_FWD, SPEED_FWD); break;
    case Qt::Key_S: setLocalCommand(-SPEED_FWD, -SPEED_FWD); break;
    case Qt::Key_A: setLocalCommand(-SPEED_TURN, SPEED_TURN); break;
    case Qt::Key_D: setLocalCommand(SPEED_TURN, -SPEED_TURN); break;
    default: QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *event)
{
    if(event->isAutoRepeat()) return;

    // Reset to 0 when WASD keys are released
    if(event->key() == Qt::Key_W || event->key() == Qt::Key_S ||
        event->key() == Qt::Key_A || event->key() == Qt::Key_D) {
        setLocalCommand(0.0, 0.0);
    }
}

// --- UDP Stub (Unchanged) ---
void MainWindow::onUdpDataReady()
{
    while (udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = udpSocket->receiveDatagram();
        // Process image data here
    }
}

void MainWindow::onAutoModeToggled(bool checked)
{
    isAuto = checked;
    // Assuming Python side expects "AUTO ON" or "AUTO OFF"
    // Note: The new refactored Python script does NOT have AUTO logic,
    // but if we were sending it, it would look like this:
    /*
    if (tcpSocket->state() == QAbstractSocket::ConnectedState) {
        QString cmd = checked ? "AUTO ON\n" : "AUTO OFF\n";
        tcpSocket->write(cmd.toUtf8());
    }
    */
}
