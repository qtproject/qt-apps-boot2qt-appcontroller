#include "daemon.h"
#include "protocol.h"
#include "app.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>

Daemon::Daemon(QObject *parent)
    : QObject(parent)
    , mServer(new QTcpServer(this))
    , mClient(0)
    , mProtocol(new Protocol(this))
    , mApp(new App(this))
    , mPort(10066)
{
    loadDefaults();

    if (!mServer->listen(QHostAddress::Any, mPort))
        qDebug() << "Could not listen:" << mServer->errorString();

    connect(mServer, SIGNAL(newConnection()), this, SLOT(newConnection()));
    connect(mProtocol, SIGNAL(commandReceived(const QStringList &)), this, SLOT(printCommand(const QStringList &)));
    connect(mProtocol, SIGNAL(commandReceived(const QStringList &)), this, SLOT(handleCommands(const QStringList &)));
    connect(mApp, SIGNAL(started()), this, SLOT(appStarted()));
    connect(mApp, SIGNAL(stopped(int,int)), this, SLOT(appStopped(int,int)));
    connect(mApp, SIGNAL(stdOut(const QByteArray&)), this, SLOT(appStdout(const QByteArray&)));
    connect(mApp, SIGNAL(stdErr(const QByteArray&)), this, SLOT(appStderr(const QByteArray&)));
    connect(mApp, SIGNAL(debugging(quint16)), this, SLOT(appDebugging(quint16)));
    connect(mApp, SIGNAL(error(const QString&)), this, SLOT(appError(const QString&)));

    if (!defaultApp.isEmpty())
        mApp->start(defaultApp, defaultArgs);
}

Daemon::~Daemon()
{
}

void Daemon::newConnection()
{
    if (!mServer->hasPendingConnections())
        return;

    clientDisconnect();
    mClient = mServer->nextPendingConnection();
    connect(mClient, SIGNAL(disconnected()), this, SLOT(clientDisconnect()));
    connect(mClient, SIGNAL(readyRead()), this, SLOT(clientRead()));
}

void Daemon::printCommand(const QStringList &l)
{
    qDebug() << l;
}

void Daemon::clientRead()
{
    mProtocol->write(mClient->readAll());
}

void Daemon::clientDisconnect()
{
    delete mClient;
    mClient = 0;
}

void Daemon::handleCommands(const QStringList &list)
{
    if (list.size() == 0) {
        qDebug() << "no commands";
        return;
    }

    const QString command(list.first());
    if (command == "start") {
        if (list.size() < 2) {
            qDebug() << "too less options";
            return;
        }
        mApp->start(list[1], defaultArgs + list.mid(2));
    } else if (command == "stop") {
        mApp->stop();
    } else if (command == "debug") {
        qDebug() << "debug not implemented";
    } else if (command == "stdin") {
        if(list.size() < 2) {
            qDebug() << "too less options";
            return;
        }
        mApp->write(list[1].toLocal8Bit());
    } else if (command == "env") {
        if(list.size() == 2) {
            mApp->delEnv(list[1]);
        } else if (list.size() == 3) {
            mApp->addEnv(list[1], list[2]);
        } else {
            qDebug() << "Invalid arg count for env";
            return;
        }
    } else {
        qDebug() << "unknown command:" << list[0];
    }

}

void Daemon::appStarted()
{
    if (!mClient)
        return;
    QStringList l("started");
    mClient->write(mProtocol->sendToClient(l).toLocal8Bit());
}

void Daemon::appStopped(int exitStatus, int exitCode)
{
    if (!mClient)
        return;
    QStringList l("stopped");
    l += QString::number(exitStatus);
    l += QString::number(exitCode);

    mClient->write(mProtocol->sendToClient(l).toLocal8Bit());
}

void Daemon::appStdout(const QByteArray &data)
{
    if (!mClient)
        return;
    QStringList l("stdout");
    l += QString::fromLocal8Bit(data);
    mClient->write(mProtocol->sendToClient(l).toLocal8Bit());
}

void Daemon::appStderr(const QByteArray &data)
{
    if (!mClient)
        return;
    QStringList l("stderr");
    l += data;
    mClient->write(mProtocol->sendToClient(l).toLocal8Bit());
}

void Daemon::appDebugging(quint16 port)
{
    if (!mClient)
        return;
    QStringList l("debugging");
    l += QString::number(port);
    mClient->write(mProtocol->sendToClient(l).toLocal8Bit());
}

void Daemon::appError(const QString &text)
{
    if (!mClient)
        return;
    QStringList l("error");
    l += text;
    mClient->write(mProtocol->sendToClient(l).toLocal8Bit());
}

void Daemon::loadDefaults()
{
    QFile f("/system/bin/appdaemon.conf");

    if (!f.open(QFile::ReadOnly))
        qFatal("Could not read config file.");

    while (!f.atEnd()) {
        QString line = f.readLine();
        if (line.startsWith("start=")) {
            defaultApp = line.mid(6).simplified();
            qDebug() << "start=" << defaultApp;
        } else if (line.startsWith("port=")) {
            mPort = line.mid(5).simplified().toUInt();
            if (mPort == 0)
                qFatal("Invalid port");
        } else if (line.startsWith("env=")) {
                QString sub = line.mid(4).simplified();
                int index = sub.indexOf('=');
                if (index < 2) {
                    // ignore
                } else {
                    mApp->addEnv(sub.left(index), sub.mid(index+1));
                    qDebug() << sub.left(index) << sub.mid(index+1);
                }
        } else if (line.startsWith("append=")) {
              defaultArgs += line.mid(7).simplified();
              qDebug() << defaultArgs;
        }
    }

    // start=/usr/bin/launcher
    // port=10066
    // env=...
    // append=...
}
