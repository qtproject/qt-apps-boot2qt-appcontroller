#include <QObject>
#include <QProcess>

class QSocketNotifier;

class Process : public QObject
{
    Q_OBJECT
public:
    Process();
    virtual ~Process();
    void start(const QStringList &args);
    void setSocketNotifier(QSocketNotifier*);
    void setDebug();
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
};
