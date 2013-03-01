#include "app.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QDebug>

App::App(QObject *parent)
    : QObject(parent)
    , mProcess(new QProcess(this))
    , mEnv(new QProcessEnvironment(QProcessEnvironment::systemEnvironment()))
{
    mProcess->setProcessChannelMode(QProcess::SeparateChannels);

    connect(mProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
    connect(mProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(processFinished(int, QProcess::ExitStatus)));
    connect(mProcess, SIGNAL(readyReadStandardError()), this, SLOT(processStderr()));
    connect(mProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(processStdout()));
    connect(mProcess, SIGNAL(started()), this, SLOT(processStarted()));
}

App::~App()
{
    delete mEnv;
}

void App::start(const QString &binary, const QStringList &args)
{
    stop(); 
    mProcess->setProcessEnvironment(*mEnv);
    mProcess->start(binary, args);
}

void App::stop()
{
    if (mProcess->state() == QProcess::NotRunning)
        return;

    mProcess->terminate();
    if (!mProcess->waitForFinished()) {
        mProcess->kill();
        if (!mProcess->waitForFinished()) {
            emit error("Could not kill");
        }
    }
}

void App::debug()
{
    emit error("Debugging not implemented.");
}

void App::addEnv(const QString &key, const QString &value)
{
    mEnv->insert(key, value);
}

void App::delEnv(const QString &key)
{
    mEnv->remove(key);
}

void App::write(const QByteArray &data)
{
   if (mProcess->state() == QProcess::Running)
       mProcess->write(data);
   else
      emit error("Could not write input: Process not running.");
}

void App::processError(QProcess::ProcessError err)
{
    switch ( err ) {
        case QProcess::FailedToStart:
            emit error("Process failed to start.");
            break;
        case QProcess::Crashed:
              // no error
              // will be handled by: processFinished(...)
            break;
        case QProcess::Timedout: emit error("Last waitFor... timed out."); break;
        case QProcess::WriteError: emit error("Error during write to process."); break;
        case QProcess::ReadError: emit error("Error during read from process."); break;
        case QProcess::UnknownError: emit error("Process had an unknown error."); break;
    }
}

void App::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::NormalExit) {
        qDebug() << "Process exited with exitcode" << exitCode;
    } else {
        qDebug() << "Process crashed";
    }
    emit stopped(exitStatus, exitCode);
}

void App::processStderr()
{
    QByteArray out = mProcess->readAllStandardError();
    if (!out.isEmpty())
        emit stdErr(out);
}

void App::processStdout()
{
    QByteArray out = mProcess->readAllStandardOutput();
    if (!out.isEmpty())
        emit stdOut(out);
}

void App::processStarted()
{
    emit started();
    qDebug() << "Process started";
}

