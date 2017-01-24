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

#include "process.h"
#include <QCoreApplication>
#include <unistd.h>
#include <QDebug>
#include <QFile>
#include <QLocalSocket>
#include <QSocketNotifier>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>
#include <QFileInfo>
#include <QTcpSocket>
#include <errno.h>

bool parseConfigFileDirectory(Config *config, const QString &dirName);
static int pipefd[2];

static void signalhandler(int)
{
    write(pipefd[1], " ", 1);
}

static bool analyzeBinary(const QString &binary)
{
    QFileInfo fi(binary);
    if (!fi.exists()) {
        printf("Binary does not exist.\n");
        return false;
    }
    if (!fi.isFile()) {
        printf("Binary is not a file.\n");
        return false;
    }
    if (!fi.isReadable()) {
        printf("Binary is not readable.\n");
        return false;
    }
    if (!fi.isExecutable()) {
        printf("Binary is not executable.\n");
        return false;
    }

    if (fi.size() < 4) {
        printf("Binary is smaller than 4 bytes.\n");
        return false;
    }

    QFile f(binary);
    if (!f.open(QFile::ReadOnly)) {
        printf("Could not open binary to analyze.\n");
        return false;
    }

    QByteArray elfHeader = f.read(4);
    f.close();

    if (elfHeader.size() < 4) {
        printf("Failed to read ELF header.\n");
        return false;
    }

    if (elfHeader != QByteArray::fromHex("7f454C46")) { // 0x7f ELF
        printf("Binary is not an ELF file.\n");
        return false;
    }

    return true;
}

Process::Process()
    : QObject(0)
    , mProcess(new QProcess(this))
    , mDebuggee(0)
    , mDebug(false)
    , mStdoutFd(1)
    , mBeingRestarted(false)
{
    mProcess->setProcessChannelMode(QProcess::SeparateChannels);
    connect(mProcess, &QProcess::readyReadStandardError, this, &Process::readyReadStandardError);
    connect(mProcess, &QProcess::readyReadStandardOutput, this, &Process::readyReadStandardOutput);
    connect(mProcess, (void (QProcess::*)(int, QProcess::ExitStatus))&QProcess::finished, this, &Process::finished);
    connect(mProcess, (void (QProcess::*)(QProcess::ProcessError))&QProcess::error, this, &Process::error);

    if (pipe2(pipefd, O_CLOEXEC) != 0)
        qWarning("Could not create pipe");

    QSocketNotifier *n = new QSocketNotifier(pipefd[0], QSocketNotifier::Read, this);
    connect(n, SIGNAL(activated(int)), this, SLOT(stop()));

    signal(SIGINT, signalhandler);
    signal(SIGTERM, signalhandler);
    signal(SIGHUP, signalhandler);
    signal(SIGPIPE, signalhandler);
}

Process::~Process()
{
    close(pipefd[0]);
    close(pipefd[1]);
}

void Process::forwardProcessOutput(qintptr fd, const QByteArray &data)
{
    const char *constData = data.constData();
    int size = data.size();
    while (size > 0) {
        int written = write(fd, constData, size);
        if (written == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                fd_set outputFdSet;
                FD_ZERO(&outputFdSet);
                FD_SET(fd, &outputFdSet);
                fd_set inputFdSet;
                FD_ZERO(&inputFdSet);
                FD_SET(pipefd[0], &inputFdSet);
                if (select(qMax(fd, static_cast<qintptr>(pipefd[0])) + 1,
                           &inputFdSet, &outputFdSet, NULL, NULL) > 0 &&
                           !FD_ISSET(pipefd[0], &inputFdSet))
                    continue;
                // else fprintf below will output the appropriate errno
            }
            fprintf(stderr, "Cannot forward application output: %d - %s\n", errno, strerror(errno));
            qApp->quit();
            break;
        }
        size -= written;
        constData += written;
    }
}

void Process::readyReadStandardOutput()
{
    forwardProcessOutput(mStdoutFd, mProcess->readAllStandardOutput());
}

void Process::readyReadStandardError()
{
    QByteArray b = mProcess->readAllStandardError();
    if (mDebug) {
        int index = b.indexOf(" created; pid = ");
        if (index >= 0) {
            mDebuggee = QString::fromLatin1(b.mid(index+16)).toUInt();
        }
        mDebug = false; // only search once
    }
    forwardProcessOutput(2, b);
}

void Process::setDebug()
{
    mDebug = true;
}

void Process::error(QProcess::ProcessError error)
{
    switch (error) {
    case QProcess::FailedToStart:
        printf("Failed to start\n");
        analyzeBinary(mBinary);
        break;
    case QProcess::Crashed:
        printf("Application crashed: %s\n", qPrintable(mBinary));
        break;
    case QProcess::Timedout:
        printf("Timedout\n");
        break;
    case QProcess::WriteError:
        printf("Write error\n");
        break;
    case QProcess::ReadError:
        printf("Read error\n");
        break;
    case QProcess::UnknownError:
        printf("Unknown error\n");
        break;
    }
    if (!mBeingRestarted)
        qApp->quit();
}

void Process::finished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::NormalExit)
        printf("Process exited with exit code %d\n", exitCode);
    else
        printf("Process stopped\n");
    if (!mBeingRestarted) {
        qDebug() << "quit";
        qApp->quit();
    }
}

void Process::startup()
{
    QProcessEnvironment pe = QProcessEnvironment::systemEnvironment();
    QStringList args = mStartupArguments;
    mBeingRestarted = false;

    Config actualConfig = mConfig;

    // Parse temporary config files
    // This needs to be done on every startup because those files are expected to change.
    parseConfigFileDirectory(&actualConfig, "/var/lib/b2qt/appcontroller.conf.d");
    parseConfigFileDirectory(&actualConfig, "/tmp/b2qt/appcontroller.conf.d");

    foreach (const QString &key, actualConfig.env.keys()) {
        if (!pe.contains(key)) {
            qDebug() << key << actualConfig.env.value(key);
            pe.insert(key, actualConfig.env.value(key));
        }
    }
    if (!actualConfig.base.isEmpty())
        pe.insert(QLatin1String("B2QT_BASE"), actualConfig.base);
    if (!actualConfig.platform.isEmpty())
        pe.insert(QLatin1String("B2QT_PLATFORM"), actualConfig.platform);

    args.append(actualConfig.args);

    mProcess->setProcessEnvironment(pe);
    mBinary = args.first();
    args.removeFirst();
    qDebug() << mBinary << args;
    mProcess->start(mBinary, args);
}

void Process::start(const QStringList &args)
{
    mStartupArguments = args;
    startup();
}

void Process::stop()
{
    if (mProcess->state() == QProcess::QProcess::NotRunning) {
        printf("No process running\n");
        if (!mBeingRestarted)
            qApp->exit();
        return;
    }

    if (mDebuggee != 0) {
        qDebug() << "Kill debuggee " << mDebuggee;
        if (kill(mDebuggee, SIGKILL) != 0)
            perror("Could not kill debugee");
    }

    mProcess->terminate();
    if (!mProcess->waitForFinished())
        mProcess->kill();
}

void Process::stopForRestart()
{
    printf("Stopping application for restart\n");
    mBeingRestarted = true;
    stop();
}

void Process::restart()
{
    printf("Restarting application\n");
    mBeingRestarted = true;
    stop();
    startup();
}

void Process::incomingConnection(int i)
{
    int fd = accept(i, NULL, NULL);
    if (fd < 0 ) {
        perror("Could not accept connection");
        stop();
        return;
    }

    QLocalSocket localSocket;
    if (!localSocket.setSocketDescriptor(fd)) {
        fprintf(stderr, "Could not initialize local socket from descriptor.\n");
        close(fd);
        stop();
        return;
    }

    if (!localSocket.waitForReadyRead()) {
        fprintf(stderr, "No command received.\n");
        stop(); // default
        return;
    }

    QByteArray command = localSocket.readAll();

    if (command == "stop")
        stop();
    else if (command == "restart")
        restart();
    else if (command == "stopForRestart")
        stopForRestart();
    else
        stop();
}

void Process::setSocketNotifier(QSocketNotifier *s)
{
    connect(s, &QSocketNotifier::activated, this, &Process::incomingConnection);
}

void Process::setConfig(const Config &config)
{
    mConfig = config;
}

void Process::setStdoutFd(qintptr stdoutFd)
{
    mStdoutFd = stdoutFd;
}
