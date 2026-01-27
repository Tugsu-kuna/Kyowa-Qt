#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QNetworkDatagram>

// Configuration
const int HEARTBEAT_INTERVAL_MS = 100;
const double SPEED_FWD = 0.5;
const double SPEED_TURN = 0.3;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    tcpSocket = new QTcpSocket(this);
    udpSocket = new QUdpSocket(this);

    heartbeatTimer = new QTimer(this);
    heartbeatTimer->setInterval(HEARTBEAT_INTERVAL_MS);

    // Initialize Sliders (Ensure these exist in your .ui file)
    // Range 15-165 per ESP32 constraints
    ui->panSlider->setRange(15, 165);
    ui->panSlider->setValue(90);

    ui->tiltSlider->setRange(15, 165);
    ui->tiltSlider->setValue(90);

    setupConnections();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupConnections()
{
    // TCP & UDP Signals (Unchanged)
    connect(tcpSocket, &QTcpSocket::connected, this, &MainWindow::onTcpConnected);
    connect(tcpSocket, &QTcpSocket::disconnected, this, &MainWindow::onTcpDisconnected);
    connect(tcpSocket, &QTcpSocket::errorOccurred, this, &MainWindow::onTcpError);
    connect(udpSocket, &QUdpSocket::readyRead, this, &MainWindow::onUdpDataReady);

    // Buttons (Unchanged)
    connect(ui->btnConnect, &QPushButton::clicked, this, &MainWindow::onConnectButtonClicked);
    connect(ui->checkAutoMode, &QCheckBox::toggled, this, &MainWindow::onAutoModeToggled);

    QList<QPushButton*> buttons = {ui->btnForward, ui->btnBackward, ui->btnLeft, ui->btnRight};
    for(auto btn : buttons) {
        connect(btn, &QPushButton::pressed, this, &MainWindow::onMovePressed);
        connect(btn, &QPushButton::released, this, &MainWindow::onMoveReleased);
    }

    // --- CAMERA SLIDERS (NEW) ---
    // Send command whenever slider moves
    connect(ui->panSlider, &QSlider::valueChanged, this, &MainWindow::onCamSliderChanged);
    connect(ui->tiltSlider, &QSlider::valueChanged, this, &MainWindow::onCamSliderChanged);

    connect(heartbeatTimer, &QTimer::timeout, this, &MainWindow::sendCurrentCommand);
}

// --- NEW CAMERA LOGIC ---

void MainWindow::onCamSliderChanged()
{
    camPan = ui->panSlider->value();
    camTilt = ui->tiltSlider->value();


    sendCamCommand();
}

void MainWindow::sendCamCommand()
{
    if (tcpSocket->state() != QAbstractSocket::ConnectedState) return;

    // Protocol: CAM <pan> <tilt>\n
    QString cmd = QString("CAM %1 %2\n").arg(camPan).arg(camTilt);
    tcpSocket->write(cmd.toUtf8());
}

// --- EXISTING LOGIC BELOW (UNTOUCHED) ---

void MainWindow::onConnectButtonClicked() {
    if (tcpSocket->state() == QAbstractSocket::UnconnectedState) {
        tcpSocket->connectToHost(ui->editIpAddress->text(), ui->spinPort->value());
    } else {
        tcpSocket->disconnectFromHost();
    }
}

void MainWindow::onTcpConnected() {
    ui->lblStatus->setText("Connected");
    ui->btnConnect->setText("Disconnect");
    heartbeatTimer->start();
}

void MainWindow::onTcpDisconnected() {
    ui->lblStatus->setText("Disconnected");
    ui->btnConnect->setText("Connect");
    heartbeatTimer->stop();
}

void MainWindow::onTcpError(QAbstractSocket::SocketError) {
    ui->lblStatus->setText("Error: " + tcpSocket->errorString());
    heartbeatTimer->stop();
}

void MainWindow::onMovePressed() {
    QObject* senderObj = sender();
    if (senderObj == ui->btnForward) setLocalCommand(SPEED_FWD, SPEED_FWD);
    else if (senderObj == ui->btnBackward) setLocalCommand(-SPEED_FWD, -SPEED_FWD);
    else if (senderObj == ui->btnLeft) setLocalCommand(-SPEED_TURN, SPEED_TURN);
    else if (senderObj == ui->btnRight) setLocalCommand(SPEED_TURN, -SPEED_TURN);
}

void MainWindow::onMoveReleased() {
    setLocalCommand(0.0, 0.0);
}

void MainWindow::setLocalCommand(double left, double right) {
    targetLeft = left;
    targetRight = right;
    sendCurrentCommand();
}

void MainWindow::sendCurrentCommand() {
    if (tcpSocket->state() != QAbstractSocket::ConnectedState) return;
    QString command;
    if (targetLeft == 0.0 && targetRight == 0.0) {
        command = "STOP\n";
    } else {
        command = QString("WHEELS %1 %2\n").arg(targetLeft, 0, 'f', 2).arg(targetRight, 0, 'f', 2);
    }
    tcpSocket->write(command.toUtf8());
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    if(event->isAutoRepeat()) return;
    switch(event->key()) {
    case Qt::Key_W: setLocalCommand(SPEED_FWD, SPEED_FWD); break;
    case Qt::Key_S: setLocalCommand(-SPEED_FWD, -SPEED_FWD); break;
    case Qt::Key_A: setLocalCommand(-SPEED_TURN, SPEED_TURN); break;
    case Qt::Key_D: setLocalCommand(SPEED_TURN, -SPEED_TURN); break;
    default: QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *event) {
    if(event->isAutoRepeat()) return;
    if(event->key() == Qt::Key_W || event->key() == Qt::Key_S ||
        event->key() == Qt::Key_A || event->key() == Qt::Key_D) {
        setLocalCommand(0.0, 0.0);
    }
}

void MainWindow::onUdpDataReady() {
    while (udpSocket->hasPendingDatagrams()) {
        udpSocket->receiveDatagram();
    }
}

void MainWindow::onAutoModeToggled(bool checked) {

}
