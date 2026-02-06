#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QNetworkDatagram>
#include <QPixmap>

// Configuration
const int HEARTBEAT_INTERVAL_MS = 100;
const double SPEED_FWD = 0.5;
const double SPEED_TURN = 0.3;
const int UDP_VIDEO_PORT = 8000;
const int CAM_STEP = 15; // Kept your faster speed (15)
const int CAM_MIN = 15;
const int CAM_MAX = 165;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // --- 1. FIX FOCUS STEALING ---
    // Prevent buttons/sliders from keeping focus after being clicked.
    ui->panSlider->setFocusPolicy(Qt::NoFocus);
    ui->tiltSlider->setFocusPolicy(Qt::NoFocus);
    ui->checkAutoMode->setFocusPolicy(Qt::NoFocus);
    ui->btnConnect->setFocusPolicy(Qt::NoFocus);

    // Motor Buttons
    ui->btnForward->setFocusPolicy(Qt::NoFocus);
    ui->btnBackward->setFocusPolicy(Qt::NoFocus);
    ui->btnLeft->setFocusPolicy(Qt::NoFocus);
    ui->btnRight->setFocusPolicy(Qt::NoFocus);

    // Note: We cannot set NoFocus on editIpAddress, otherwise you can't type in it!
    // We handle that in onConnectButtonClicked instead.

    tcpSocket = new QTcpSocket(this);
    udpSocket = new QUdpSocket(this);

    if (udpSocket->bind(QHostAddress::Any, UDP_VIDEO_PORT)) {
        qDebug() << "UDP Video Listener Active on Port" << UDP_VIDEO_PORT;
    } else {
        qDebug() << "FAILED to bind UDP Port!";
    }

    heartbeatTimer = new QTimer(this);
    heartbeatTimer->setInterval(HEARTBEAT_INTERVAL_MS);

    // Initialize State
    camPan = 90;
    camTilt = 90;

    // Initialize Sliders
    ui->panSlider->setRange(CAM_MIN, CAM_MAX);
    ui->panSlider->setValue(camPan);

    ui->tiltSlider->setRange(CAM_MIN, CAM_MAX);
    ui->tiltSlider->setValue(camTilt);

    ui->lblVideo->setText("WAITING FOR VIDEO...");
    ui->lblVideo->setStyleSheet("background-color: black; color: white;");
    ui->lblVideo->setAlignment(Qt::AlignCenter);

    setupConnections();

    // Start with focus on the main window so keys work immediately
    this->setFocus();
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

    // Slider User Interaction
    connect(ui->panSlider, &QSlider::valueChanged, this, &MainWindow::onCamSliderChanged);
    connect(ui->tiltSlider, &QSlider::valueChanged, this, &MainWindow::onCamSliderChanged);

    connect(heartbeatTimer, &QTimer::timeout, this, &MainWindow::sendCurrentCommand);
}

// --- VIDEO ---
void MainWindow::onUdpDataReady()
{
    while (udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = udpSocket->receiveDatagram();
        QByteArray data = datagram.data();
        QPixmap pixmap;
        if (pixmap.loadFromData(data, "JPG")) {
            ui->lblVideo->setPixmap(pixmap.scaled(ui->lblVideo->size(),
                                                  Qt::KeepAspectRatio,
                                                  Qt::SmoothTransformation));
        }
    }
}

// --- SLIDERS ---
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

// --- CONNECTION ---
void MainWindow::onConnectButtonClicked() {
    // 2. FORCE FOCUS AWAY FROM IP BOX
    // When you click connect, we assume you are done typing.
    // We clear focus from the text box and give it to the main window.
    ui->editIpAddress->clearFocus();
    ui->spinPort->clearFocus();
    this->setFocus();

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

    // Ensure focus is on the window again just in case
    this->setFocus();
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

// --- MOTOR LOGIC ---
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

// --- KEYBOARD CONTROLS ---
void MainWindow::keyPressEvent(QKeyEvent *event) {

    bool camChanged = false;

    switch(event->key()) {

    // MOTORS (WASD) - No AutoRepeat
    case Qt::Key_W:
        if(event->isAutoRepeat()) return;
        setLocalCommand(SPEED_FWD, SPEED_FWD);
        break;
    case Qt::Key_S:
        if(event->isAutoRepeat()) return;
        setLocalCommand(-SPEED_FWD, -SPEED_FWD);
        break;
    case Qt::Key_A:
        if(event->isAutoRepeat()) return;
        setLocalCommand(-SPEED_TURN, SPEED_TURN);
        break;
    case Qt::Key_D:
        if(event->isAutoRepeat()) return;
        setLocalCommand(SPEED_TURN, -SPEED_TURN);
        break;

    // CAMERA (ARROWS) - Allow AutoRepeat
    case Qt::Key_Left:
        camPan += CAM_STEP; // Check if this direction feels right, swap += and -= if needed
        camChanged = true;
        break;
    case Qt::Key_Right:
        camPan -= CAM_STEP;
        camChanged = true;
        break;
    case Qt::Key_Up:
        camTilt += CAM_STEP;
        camChanged = true;
        break;
    case Qt::Key_Down:
        camTilt -= CAM_STEP;
        camChanged = true;
        break;

    default:
        QMainWindow::keyPressEvent(event);
        return;
    }

    if (camChanged) {
        // Clamp
        if (camPan > CAM_MAX) camPan = CAM_MAX;
        if (camPan < CAM_MIN) camPan = CAM_MIN;
        if (camTilt > CAM_MAX) camTilt = CAM_MAX;
        if (camTilt < CAM_MIN) camTilt = CAM_MIN;

        // Send
        sendCamCommand();

        // Update Visuals (Blocked)
        ui->panSlider->blockSignals(true);
        ui->panSlider->setValue(camPan);
        ui->panSlider->blockSignals(false);

        ui->tiltSlider->blockSignals(true);
        ui->tiltSlider->setValue(camTilt);
        ui->tiltSlider->blockSignals(false);
    }
}

void MainWindow::keyReleaseEvent(QKeyEvent *event) {
    if(event->isAutoRepeat()) return;

    if(event->key() == Qt::Key_W || event->key() == Qt::Key_S ||
        event->key() == Qt::Key_A || event->key() == Qt::Key_D) {
        setLocalCommand(0.0, 0.0);
    }
}

void MainWindow::onAutoModeToggled(bool checked) {
    if (tcpSocket->state() != QAbstractSocket::ConnectedState) return;
    QString cmd = checked ? "SENTRY ON\n" : "SENTRY OFF\n";
    tcpSocket->write(cmd.toUtf8());
}
