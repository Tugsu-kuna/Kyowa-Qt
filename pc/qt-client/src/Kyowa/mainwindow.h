#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QKeyEvent>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    // Network
    void onTcpConnected();
    void onTcpDisconnected();
    void onTcpError(QAbstractSocket::SocketError socketError);
    void onUdpDataReady();

    // UI
    void onConnectButtonClicked();
    void onAutoModeToggled(bool checked);
    void onMovePressed();
    void onMoveReleased();

    // Camera Slots (NEW)
    void onCamSliderChanged();

    // Heartbeat
    void sendCurrentCommand();

private:
    Ui::MainWindow *ui;
    QTcpSocket *tcpSocket;
    QUdpSocket *udpSocket;
    QTimer *heartbeatTimer;

    // Robot State
    double targetLeft = 0.0;
    double targetRight = 0.0;

    // Camera State (NEW)
    int camPan = 90;
    int camTilt = 90;

    void setupConnections();
    void setLocalCommand(double left, double right);
    void sendCamCommand(); // Helper for camera
};
#endif // MAINWINDOW_H
