#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QTimer>
#include <QKeyEvent> // Improvement: Keyboard support

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
    // Improvement: Key press events for WASD control
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    // Network Slots
    void onTcpConnected();
    void onTcpDisconnected();
    void onTcpError(QAbstractSocket::SocketError socketError);
    void onUdpDataReady();

    // UI Slots
    void onConnectButtonClicked();
    void onAutoModeToggled(bool checked);

    // Control Slots (Pressed/Released logic)
    void onMovePressed();
    void onMoveReleased();

    // The Heartbeat Loop
    void sendCurrentCommand();

private:
    Ui::MainWindow *ui;
    QTcpSocket *tcpSocket;
    QUdpSocket *udpSocket;
    QTimer *heartbeatTimer;

    // Robot State
    double targetLeft = 0.0;
    double targetRight = 0.0;
    bool isAuto = false;

    void setupConnections();
    void setLocalCommand(double left, double right);
};
#endif // MAINWINDOW_H
