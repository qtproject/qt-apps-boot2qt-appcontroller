#include <QCoreApplication>
#include <QFile>
#include <QDebug>
#include <errno.h>
#include <unistd.h>
#include <QStringList>
#include <fcntl.h>

#define PID_FILE "/data/user/.appcontroller"

static void loadDefaults(QStringList &defaultArgs)
{
    QFile f("/system/bin/appcontroller.conf");

    if (!f.open(QFile::ReadOnly)) {
        qWarning("Could not read config file.");
        return;
    }

    while (!f.atEnd()) {
        QString line = f.readLine();
        if (line.startsWith("env=")) {
                QString sub = line.mid(4).simplified();
                int index = sub.indexOf('=');
                if (index < 2) {
                    // ignore
                } else {
                    setenv(sub.left(index).toLocal8Bit().constData(), sub.mid(index+1).toLocal8Bit().constData(), 1);
                    qDebug() << sub.left(index) << sub.mid(index+1);
                }
        } else if (line.startsWith("append=")) {
              defaultArgs += line.mid(7).simplified();
              qDebug() << defaultArgs;
        }
    }

    // env=...
    // append=...
}

static pid_t lastPID(QFile &f)
{
    f.seek(0);
    bool ok;
    pid_t pid = f.readAll().toUInt(&ok);
    if (!ok) {
        qWarning("Invalid last PID.");
        return 0;
    }

    return pid;
}

static void stop(QFile &file)
{
    pid_t pid = lastPID(file);
    if (pid == 0)
        return;

    int rc = ::kill(pid, SIGTERM);
    if (rc != 0) {
        if (errno == ESRCH)
            return;
        else {
            qWarning("Kill not permitted/invalid");
            return;
        }
    }

    sleep(1);

    rc = ::kill(pid, SIGKILL);
    if (rc != 0) {
        if (errno == ESRCH)
            return;
        else {
            qWarning("Kill not permitted/invalid");
            return;
        }
    }
}

int lockFile(int handle)
{
    struct flock lock;
    lock.l_type = F_WRLCK | F_RDLCK;
    lock.l_whence = SEEK_SET;
    lock.l_start = 0;
    lock.l_len = 100;
    int rc = fcntl(handle, F_SETLKW, &lock);
    if (rc != 0) {
        perror("Locking failed");
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QStringList defaultArgs;
    QString binary;

    QStringList args = app.arguments();
    args.removeFirst();
    if (args.size() == 0) {
        qWarning("No arguments given.");
        return 1;
    }

    QFile f(PID_FILE);
    if (!f.open(QFile::ReadWrite)) {
        qDebug() << "Could not open PID file.";
        return 1;
    }

    if (lockFile(f.handle()) != 0) {
        qDebug() << "Could not get lock.";
        return 1;
    }
    qDebug() << "File locked";

    while (!args.isEmpty()) {
        if (args[0] == "--start") {
            if (args.size() < 2) {
                qWarning("--start requires and argument");
                return 1;
            }
            binary = args[1];
            args.removeFirst();
            if (binary.isEmpty()) {
                qWarning("App path is empty");
                return 1;
            }
            stop(f);
            loadDefaults(defaultArgs);
        } else if (args[0] == "--stop") {
            stop(f);
            return 0;
        } else {
            qWarning("unknown argument: %s", args.first().toLocal8Bit().constData());
            return 1;
        }
        args.removeFirst();
    }

    defaultArgs.push_front(binary);

    char **arglist = new char*[defaultArgs.size()+1];
    for (int i = 0; i < defaultArgs.size(); i++) {
        arglist[i] = strdup(defaultArgs[i].toLocal8Bit().constData());
    }
    arglist[defaultArgs.size()] = 0;
    defaultArgs.clear();

    if (!f.seek(0)) {
        qDebug() << "Could not seek.";
        return 1;
    }
    if (!f.resize(0)) {
        qDebug() << "Could not resize.";
        return 1;
    }

    QByteArray data = QString::number(getpid()).toLatin1();

    if (f.write(data) != data.size()) {
        qDebug() << "Write failed.";
        return 1;
    }
    f.close();
    qDebug() << "Starting binary";

    execv(binary.toLocal8Bit().constData(), arglist);

    return 0;
}
