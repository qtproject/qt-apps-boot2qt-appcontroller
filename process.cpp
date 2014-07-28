/****************************************************************************
**
** Copyright (C) 2014 Digia Plc
** All rights reserved.
** For any questions to Digia, please use contact form at http://qt.digia.com
**
** This file is part of QtEnterprise Embedded.
**
** Licensees holding valid Qt Enterprise licenses may use this file in
** accordance with the Qt Enterprise License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.
**
** If you have questions regarding the use of this file, please use
** contact form at http://qt.digia.com
**
****************************************************************************/

#include "process.h"
#include <QCoreApplication>
#include <unistd.h>
#include <QDebug>
#include <QFile>
#include <QSocketNotifier>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>
#include <QFileInfo>

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
{
    mProcess->setProcessChannelMode(QProcess::SeparateChannels);
    connect(mProcess, &QProcess::readyReadStandardError, this, &Process::readyReadStandardError);
    connect(mProcess, &QProcess::readyReadStandardOutput, this, &Process::readyReadStandardOutput);
    connect(mProcess, (void (QProcess::*)(int, QProcess::ExitStatus))&QProcess::finished, this, &Process::finished);
    connect(mProcess, (void (QProcess::*)(QProcess::ProcessError))&QProcess::error, this, &Process::error);
    connect(mProcess, (void (QProcess::*)(int, QProcess::ExitStatus))&QProcess::finished, qApp, &QCoreApplication::quit);

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

void Process::readyReadStandardOutput()
{
    QByteArray b = mProcess->readAllStandardOutput();
    write(1, b.constData(), b.size());

    if (mConfig.flags.testFlag(Config::PrintDebugMessages))
        qDebug() << b;
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
    write(2, b.constData(), b.size());

    if (mConfig.flags.testFlag(Config::PrintDebugMessages))
        qDebug() << b;
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
    qApp->quit();
}

void Process::finished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::NormalExit)
        printf("Process exited with exit code %d\n", exitCode);
    else
        printf("Process stopped\n");
}

void Process::startup(QStringList args)
{
#ifdef Q_OS_ANDROID
    QProcessEnvironment pe = interactiveProcessEnvironment();
#else
    QProcessEnvironment pe = QProcessEnvironment::systemEnvironment();
#endif

    foreach (const QString &key, mConfig.env.keys()) {
        if (!pe.contains(key)) {
            qDebug() << key << mConfig.env.value(key);
            pe.insert(key, mConfig.env.value(key));
        }
    }
    if (!mConfig.base.isEmpty())
        pe.insert(QLatin1String("B2QT_BASE"), mConfig.base);
    if (!mConfig.platform.isEmpty())
        pe.insert(QLatin1String("B2QT_PLATFORM"), mConfig.platform);

    args.append(mConfig.args);

    mProcess->setProcessEnvironment(pe);
    mBinary = args.first();
    args.removeFirst();
    qDebug() << mBinary << args;
    mProcess->start(mBinary, args);
}

void Process::start(const QStringList &args)
{
    startup(args);
}

void Process::stop()
{
    if (mProcess->state() == QProcess::QProcess::NotRunning) {
        printf("No process running\n");
        qApp->exit();
        return;
    }

    if (mDebuggee != 0) {
        qDebug() << "Kill debuggee " << mDebuggee;
        if (kill(mDebuggee, SIGKILL) != 0)
            perror("Could not kill debugee");
    }
    if (kill(-getpid(), SIGTERM) != 0)
        perror("Could not kill process group");

    mProcess->terminate();
    if (!mProcess->waitForFinished())
        mProcess->kill();
}

void Process::incomingConnection(int i)
{
    accept(i, NULL, NULL);
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

QProcessEnvironment Process::interactiveProcessEnvironment() const
{
    QProcessEnvironment env;

    QProcess process;
    process.start("sh");
    if (!process.waitForStarted(3000)) {
        printf("Could not start shell.\n");
        return env;
    }

    process.write("source /system/etc/mkshrc\n");
    process.write("export -p\n");
    process.closeWriteChannel();

    printf("waiting for process to finish\n");
    if (!process.waitForFinished(1000)) {
        printf("did not finish: terminate\n");
        process.terminate();
        if (!process.waitForFinished(1000)) {
            printf("did not terminate: kill\n");
            process.kill();
            if (!process.waitForFinished(1000)) {
                printf("Could not stop process.\n");
            }
        }
    }

    QList<QByteArray> list = process.readAllStandardOutput().split('\n');
    if (list.isEmpty())
       printf("Failed to read environment output\n");

    foreach (QByteArray entry, list) {
        if (entry.startsWith("export ")) {
            entry = entry.mid(7);
        } else if (entry.startsWith("declare -x ")) {
            entry = entry.mid(11);
        } else {
            continue;
        }

        QByteArray key;
        QByteArray value;
        int index = entry.indexOf('=');

        if (index > 0) {
            key = entry.left(index);
            value = entry.mid(index + 1);
        } else {
            key = entry;
            // value is empty
        }

        // Remove simple escaping.
        // This is not complete.
        if (value.startsWith('\'') and value.endsWith('\''))
            value = value.mid(1, value.size()-2);
        else if (value.startsWith('"') and value.endsWith('"'))
            value = value.mid(1, value.size()-2);

        env.insert(key, value);
    }

    return env;
}

