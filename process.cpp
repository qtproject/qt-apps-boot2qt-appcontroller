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
        printf("Crashed\n");
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
