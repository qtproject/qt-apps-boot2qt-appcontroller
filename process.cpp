#include "process.h"
#include <QCoreApplication>
#include <unistd.h>
#include <QDebug>
#include <QFile>
#include <QSocketNotifier>
#include <sys/socket.h>
#include <signal.h>

Process::Process()
    : QObject(0)
    , mProcess(new QProcess(this))
    , mDebuggee(0)
    , mDebug(false)
{
    mProcess->setProcessChannelMode(QProcess::SeparateChannels);
    connect(mProcess, SIGNAL(readyReadStandardError()), this, SLOT(readyReadStandardError()));
    connect(mProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(readyReadStandardOutput()));
    connect(mProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(finished(int, QProcess::ExitStatus)));
    connect(mProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(error(QProcess::ProcessError)));
    connect(mProcess, SIGNAL(finished(int, QProcess::ExitStatus)), qApp, SLOT(quit()));
}

Process::~Process()
{
}

void Process::readyReadStandardOutput()
{
    QByteArray b = mProcess->readAllStandardOutput();
    write(1, b.constData(), b.size());
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
}

void Process::setDebug()
{
    mDebug = true;
}

void Process::error(QProcess::ProcessError)
{
    qDebug() << "Process error";
}

void Process::finished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::NormalExit)
        qDebug() << "Process exited with exitcode" << exitCode;
    else
        qDebug() << "Process stopped";
}

void Process::startup(QStringList args)
{
    QProcessEnvironment pe = QProcessEnvironment::systemEnvironment();

    QFile f("/system/bin/appcontroller.conf");

    if (!f.open(QFile::ReadOnly)) {
        qWarning("Could not read config file.");
    }

    while (!f.atEnd()) {
        QString line = f.readLine();
        if (line.startsWith("env=")) {
                QString sub = line.mid(4).simplified();
                int index = sub.indexOf('=');
                if (index < 2) {
                    // ignore
                } else {
                    pe.insert(sub.left(index), sub.mid(index+1));
                    qDebug() << sub.left(index) << sub.mid(index+1);
                }
        } else if (line.startsWith("append=")) {
              args += line.mid(7).simplified();
              qDebug() << args;
        }
    }

    // env=...
    // append=...

    mProcess->setProcessEnvironment(pe);
    QString binary = args.first();
    qDebug() << binary << args;
    args.removeFirst();
    mProcess->start(binary, args);
}

void Process::start(const QStringList &args)
{
    startup(args);
}

void Process::stop()
{
    if (mProcess->state() == QProcess::QProcess::NotRunning) {
        qDebug() << "No process running";
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
    connect(s, SIGNAL(activated(int)), this, SLOT(incomingConnection(int)));
}
