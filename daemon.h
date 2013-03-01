#ifndef DAEMON_H
#define DAEMON_H

#include <QObject>
#include <QStringList>
class QTcpServer;
class QTcpSocket;
class Protocol;
class App;

class Daemon : public QObject
{
Q_OBJECT

public:
    Daemon(QObject *parent = 0);
    virtual ~Daemon();

private slots:
    void newConnection();
    void printCommand(const QStringList &);
    void clientDisconnect();
    void clientRead();
    void handleCommands(const QStringList &);

    void appStarted();
    void appStopped(int, int);
    void appStdout(const QByteArray &);
    void appStderr(const QByteArray &);
    void appDebugging(quint16);
    void appError(const QString &);

private:
    void loadDefaults();

    QTcpServer *mServer;
    QTcpSocket *mClient;
    Protocol *mProtocol;
    App *mApp;

    QString defaultApp;
    QStringList defaultArgs;
    quint16 mPort;
};

#endif // DAEMON_H
