/******************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of Qt for Device Creation.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
******************************************************************************/

#ifndef PROCESS_H
#define PROCESS_H

#include <QObject>
#include <QProcess>
#include <QMap>
#include <QTcpServer>

class QSocketNotifier;

struct Config {
    enum DebugInterface{
        LocalDebugInterface,
        PublicDebugInterface
    };

    Config() : platform("unknown"), debugInterface(LocalDebugInterface) { }

    QString base;
    QString platform;
    QMap<QString,QString> env;
    QStringList args;
    DebugInterface debugInterface;
};

class Process : public QObject
{
    Q_OBJECT
public:
    Process();
    virtual ~Process();
    void start(const QStringList &args);
    void setSocketNotifier(QSocketNotifier*);
    void setDebug();
    void setConfig(const Config &);
    void setStdoutFd(qintptr stdoutFd);
public slots:
    void stop();
    void stopForRestart();
    void restart();
private slots:
    void readyReadStandardError();
    void readyReadStandardOutput();
    void finished(int, QProcess::ExitStatus);
    void error(QProcess::ProcessError);
    void incomingConnection(int);
private:
    void forwardProcessOutput(qintptr fd, const QByteArray &data);
    void startup();
    QProcess *mProcess;
    int mDebuggee;
    bool mDebug;
    Config mConfig;
    QString mBinary;
    qintptr mStdoutFd;
    QStringList mStartupArguments;
    bool mBeingRestarted;
};

#endif // PROCESS_H
