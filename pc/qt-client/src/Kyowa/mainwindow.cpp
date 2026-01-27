#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QNetworkDatagram>
#include <QPixmap> // Required for image handling

// Configuration
const int HEARTBEAT_INTERVAL_MS = 100;
const double SPEED_FWD = 0.5;
const double SPEED_TURN = 0.3;
const int UDP_VIDEO_PORT = 8000; // Port to listen for video
const int CAM_STEP = 5;          // Degrees to move per key press

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    tcpSocket = new QTcpSocket(this);
    udpSocket = new QUdpSocket(this);

    // 1. START LISTENING FOR VIDEO
    if (udpSocket->bind(QHostAddress::Any, UDP_VIDEO_PORT)) {
        qDebug() << "UDP Video Listener Active on Port" << UDP_VIDEO_PORT;
    } else {
        qDebug() << "FAILED to bind UDP Port!";
    }

    heartbeatTimer = new QTimer(this);
    heartbeatTimer->setInterval(HEARTBEAT_INTERVAL_MS);

    // Initialize Sliders
    ui->panSlider->setRange(15, 165);
    ui->panSlider->setValue(90);

    ui->tiltSlider->setRange(15, 165);
    ui->tiltSlider->setValue(90);

    // Initialize Video Label (Visual Placeholder)
    ui->lblVideo->setText("WAITING FOR VIDEO...");
    ui->lblVideo->setStyleSheet("background-color: black; color: white;");
    ui->lblVideo->setAlignment(Qt::AlignCenter);

    setupConnections();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupConnections()
{
    connect(tcpSocket, &QTcpSocket::connected, this, &MainWindow::onTcpConnected);
    connect(tcpSocket, &QTcpSocket::disconnected, this, &MainWindow::onTcpDisconnected);
    connect(tcpSocket, &QTcpSocket::errorOccurred, this, &MainWindow::onTcpError);
    connect(udpSocket, &QUdpSocket::readyRead, this, &MainWindow::onUdpDataReady);

    connect(ui->btnConnect, &QPushButton::clicked, this, &MainWindow::onConnectButtonClicked);
    connect(ui->checkAutoMode, &QCheckBox::toggled, this, &MainWindow::onAutoModeToggled);

    QList<QPushButton*> buttons = {ui->btnForward, ui->btnBackward, ui->btnLeft, ui->btnRight};
    for(auto btn : buttons) {
        connect(btn, &QPushButton::pressed, this, &MainWindow::onMovePressed);
        connect(btn, &QPushButton::released, this, &MainWindow::onMoveReleased);
    }

    connect(ui->panSlider, &QSlider::valueChanged, this, &MainWindow::onCamSliderChanged);
    connect(ui->tiltSlider, &QSlider::valueChanged, this, &MainWindow::onCamSliderChanged);

    connect(heartbeatTimer, &QTimer::timeout, this, &MainWindow::sendCurrentCommand);
}

// --- VIDEO DISPLAY LOGIC ---
void MainWindow::onUdpDataReady()
{
    while (udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = udpSocket->receiveDatagram();
        QByteArray data = datagram.data();

        // Convert raw bytes to Image
        QPixmap pixmap;
        if (pixmap.loadFromData(data, "JPG")) {
            // Scale to fit the label while keeping aspect ratio
            ui->lblVideo->setPixmap(pixmap.scaled(ui->lblVideo->size(),
                                                  Qt::KeepAspectRatio,
                                                  Qt::SmoothTransformation));
        }
    }
}

// --- CAMERA SLIDER LOGIC ---
void MainWindow::onCamSliderChanged()
{
    camPan = ui->panSlider->value();
    camTilt = ui->tiltSlider->value();
    sendCamCommand();
}

void MainWindow::sendCamCommand()
{
    if (tcpSocket->state() != QAbstractSocket::ConnectedState) return;
    QString cmd = QString("CAM %1 %2\n").arg(camPan).arg(camTilt);
    tcpSocket->write(cmd.toUtf8());
}

// --- CONNECTION & MOTOR LOGIC ---
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
    ui->lblVideo->clear();
    ui->lblVideo->setText("NO SIGNAL");
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

// --- KEYBOARD CONTROLS (WASD + ARROWS) ---
void MainWindow::keyPressEvent(QKeyEvent *event) {
    if(event->isAutoRepeat()) return;

    switch(event->key()) {
    // Motor Controls (WASD)
    case Qt::Key_W: setLocalCommand(SPEED_FWD, SPEED_FWD); break;
    case Qt::Key_S: setLocalCommand(-SPEED_FWD, -SPEED_FWD); break;
    case Qt::Key_A: setLocalCommand(-SPEED_TURN, SPEED_TURN); break;
    case Qt::Key_D: setLocalCommand(SPEED_TURN, -SPEED_TURN); break;

    // Camera Controls (Arrow Keys)
    // Directly updating the slider triggers onCamSliderChanged -> sendCamCommand
    case Qt::Key_Left:
        ui->panSlider->setValue(ui->panSlider->value() + CAM_STEP);
        break;
    case Qt::Key_Right:
        ui->panSlider->setValue(ui->panSlider->value() - CAM_STEP);
        break;
    case Qt::Key_Up:
        ui->tiltSlider->setValue(ui->tiltSlider->value() + CAM_STEP);
        break;
    case Qt::Key_Down:
        ui->tiltSlider->setValue(ui->tiltSlider->value() - CAM_STEP);
        break;

    default: QMainWindow::keyPressEvent(event);
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *event) {
    if(event->isAutoRepeat()) return;

    // Only stop motors on WASD release
    if(event->key() == Qt::Key_W || event->key() == Qt::Key_S ||
        event->key() == Qt::Key_A || event->key() == Qt::Key_D) {
        setLocalCommand(0.0, 0.0);
    }
}

void MainWindow::onAutoModeToggled(bool checked) {
    // Stub
}
