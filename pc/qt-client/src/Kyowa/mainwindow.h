#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QAbstractSocket>
#include <QNetworkDatagram>


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // UI Event Slots
    void onConnectButtonClicked();
    void onCommandButtonClicked(); // Generic for movement
    void onAutoModeToggled(bool checked);

    // TCP Networking Slots
    void onTcpConnected();
    void onTcpDisconnected();
    void onTcpError(QAbstractSocket::SocketError socketError);

    // UDP Networking Slots
    void onUdpDataReady();

private:
    Ui::MainWindow *ui;

    // Networking members
    QTcpSocket* tcpSocket;
    QUdpSocket* udpSocket;

    void setupConnections();
};
#endif // MAINWINDOW_H
