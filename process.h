#include <QObject>
#include <QProcess>
#include <QMap>

class QSocketNotifier;

struct Config {
    QString base;
    QString platform;
    QMap<QString,QString> env;
    QStringList args;
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
public slots:
    void stop();
private slots:
    void readyReadStandardError();
    void readyReadStandardOutput();
    void finished(int, QProcess::ExitStatus);
    void error(QProcess::ProcessError);
    void incomingConnection(int);
private:
    void startup(QStringList);
    QProcess *mProcess;
    int mDebuggee;
    bool mDebug;
    Config mConfig;
};
