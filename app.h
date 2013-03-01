#ifndef APP_H
#define APP_H

#include <QObject>
#include <QProcess>
class QProcessEnvironment;

class App : public QObject
{
Q_OBJECT

public:
    App(QObject *parent = 0);
    virtual ~App();

    void start(const QString &, const QStringList &);
    void stop();
    void debug();
    void addEnv(const QString &, const QString &);
    void delEnv(const QString &);
    void write(const QByteArray &);

signals:
    void started(pid_t);
    void stopped(pid_t, int, int);
    void stdOut(pid_t, const QByteArray &);
    void stdErr(pid_t, const QByteArray &);
    void debugging(pid_t, quint16);
    void error(const QString &);

private slots:
    void processError(QProcess::ProcessError error);
    void processFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void processStderr();
    void processStdout();
    void processStarted();

private:
    QProcess *mProcess;
    QProcessEnvironment *mEnv;
    pid_t mPid;
};
#endif // APP_H
