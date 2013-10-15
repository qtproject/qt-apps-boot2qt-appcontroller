#include <QObject>
#include <QProcess>
#include <QMap>

class QSocketNotifier;

struct Config {
    enum Flag {
        PrintDebugMessages = 0x01
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    enum DebugInterface{
        LocalDebugInterface,
        PublicDebugInterface
    };

    Config() : flags(0) { }

    QString base;
    QString platform;
    QMap<QString,QString> env;
    QStringList args;
    Flags flags;
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
