#include "process.h"
#include <QCoreApplication>
#include <unistd.h>
#include <QDebug>
#include <QFile>
#include <QSocketNotifier>
#include <sys/socket.h>
#include <signal.h>
#include <fcntl.h>

static int pipefd[2];

static void signalhandler(int)
{
    write(pipefd[1], " ", 1);
}

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

    foreach (const QString &key, mConfig.env.keys()) {
        qDebug() << key << mConfig.env.value(key);
        pe.insert(key, mConfig.env.value(key));
    }

    args.append(mConfig.args);
    qDebug() << args;

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

void Process::setConfig(const Config &config)
{
    mConfig = config;
}
