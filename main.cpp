/****************************************************************************
**
** Copyright (C) 2014 Digia Plc
** All rights reserved.
** For any questions to Digia, please use contact form at http://www.qt.io
**
** This file is part of Qt Enterprise Embedded.
**
** Licensees holding valid Qt Enterprise licenses may use this file in
** accordance with the Qt Enterprise License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.
**
** If you have questions regarding the use of this file, please use
** contact form at http://www.qt.io
**
****************************************************************************/

#include "process.h"
#include "portlist.h"
#include "perfprocesshandler.h"
#include <QCoreApplication>
#include <QTcpServer>
#include <QProcess>
#include <errno.h>
#include <QStringList>
#include <QSocketNotifier>
#include <QFile>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>

#define PID_FILE "/data/user/.appcontroller"

#ifdef Q_OS_ANDROID
    #define B2QT_PREFIX "/data/user/b2qt"
#else
    #define B2QT_PREFIX "/usr/bin/b2qt"
#endif

static int serverSocket = -1;

static const char socketPath[] = "#Boot2Qt_appcontroller";

static void usage()
{
    printf("appcontroller [--debug-gdb] [--debug-qml] [--port-range <range>] [--stop] [--launch] [--show-platfrom] [--make-default] [--remove-default] [--print-debug] [--version] [--detach] [executable] [arguments]\n"
           "\n"
           "--port-range <range> Port range to use for debugging connections\n"
           "--debug-gdb          Start GDB debugging\n"
           "--debug-qml          Start QML debugging\n"
           "--stop               Stop already running application\n"
           "--launch             Start application without stopping already running application\n"
           "--show-platform      Show platform information\n"
           "--make-default       Make this application the default on boot\n"
           "--remove-default     Restore the default application\n"
           "--print-debug        Print debug messages to stdout on Android\n"
           "--version            Print version information\n"
           "--detach             Start application as usual, then go into background\n"
           "--help, -h, -help    Show this help\n"
          );
}

static void setupAddressStruct(struct sockaddr_un &address)
{
    address.sun_family = AF_UNIX;
    memset(address.sun_path, 0, sizeof(address.sun_path));
    strncpy(address.sun_path, socketPath, sizeof(address.sun_path)-1);
    address.sun_path[0] = 0;
}

static int connectSocket()
{
  int create_socket;
  struct sockaddr_un address;

  if ((create_socket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
      perror("Could not create socket");
      return -1;
  }

  if (fcntl(create_socket, F_SETFD, FD_CLOEXEC) == -1) {
      perror("Unable to set CLOEXEC");
  }

  setupAddressStruct(address);

  if (connect(create_socket, (struct sockaddr *) &address, sizeof (address)) != 0) {
    perror("Could not connect");
    return -1;
  }
  close(create_socket);
  return 0;
}

static int createServerSocket()
{
  struct sockaddr_un address;

  if ((serverSocket = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
      perror("Could not create socket");
      return -1;
  }

  if (fcntl(serverSocket, F_SETFD, FD_CLOEXEC) == -1) {
      perror("Unable to set CLOEXEC");
  }

  setupAddressStruct(address);

  unsigned int tries = 20;

  while (tries > 0) {
      --tries;
      if (bind(serverSocket, (struct sockaddr *) &address, sizeof (address)) != 0) {
          if (errno != EADDRINUSE) {
              perror("Could not bind socket: App is still running");
              return -1;
          }

          if (connectSocket() != 0) {
              fprintf(stderr, "Failed to connect to process\n");
          }

          usleep(500000);
          continue;
      }

      if (listen(serverSocket, 5) != 0) {
          perror("Could not listen");
          return -1;
      }
      else
          return 0;
  }

  return -1;
}

static void stop()
{
    connectSocket();
}

static int openServer(QTcpServer *s, Utils::PortList &range)
{
    while (range.hasMore()) {
        if (s->listen(QHostAddress::Any, range.getNext()))
            return s->serverPort();
    }
    return -1;
}

static int findFirstFreePort(Utils::PortList &range)
{
    QTcpServer s;
    return openServer(&s, range);
}

static Config parseConfigFile()
{
    Config config;
    config.base = config.platform = QLatin1String("unknown");
    config.debugInterface = Config::LocalDebugInterface;

#ifdef Q_OS_ANDROID
    QFile f("/system/bin/appcontroller.conf");
#else
    QFile f("/etc/appcontroller.conf");
#endif

    if (!f.open(QFile::ReadOnly)) {
        fprintf(stderr, "Could not read config file.\n");
        return config;
    }

    while (!f.atEnd()) {
        QString line = f.readLine();
        if (line.startsWith("env=")) {
                QString sub = line.mid(4).simplified();
                int index = sub.indexOf('=');
                if (index < 2) {
                    // ignore
                } else
                    config.env[sub.left(index)] = sub.mid(index+1);
        } else if (line.startsWith("append=")) {
              config.args += line.mid(7).simplified();
        } else if (line.startsWith("base=")) {
              config.base = line.mid(5).simplified();
        } else if (line.startsWith("platform=")) {
              config.platform = line.mid(9).simplified();
        } else if (line.startsWith("debugInterface=")) {
              const QString value = line.mid(15).simplified();
              if (value == "local")
                  config.debugInterface = Config::LocalDebugInterface;
              else if (value == "public")
                  config.debugInterface = Config::PublicDebugInterface;
              else
                  qWarning() << "Unkonwn value for debuginterface:" << value;
        }
    }
    f.close();
    return config;
}

static bool removeDefault()
{
    if (QFile::exists(B2QT_PREFIX)) {
        if (!QFile::remove(B2QT_PREFIX)) {
            fprintf(stderr, "Could not remove default application.\n");
            return false;
        }
        sync();
    }
    return true;
}

static bool makeDefault(const QString &filepath)
{
    QFile executable(filepath);

    if (!executable.exists()) {
        fprintf(stderr, "File %s does not exist.\n", executable.fileName().toLocal8Bit().constData());
        return false;
    }

    if (!removeDefault())
        return false;

    if (!executable.link(B2QT_PREFIX)) {
        fprintf(stderr, "Could not link default application.\n");
        return false;
    }
    sync();
    return true;
}

static QStringList extractPerfParams(QString s)
{
    QStringList lst;
    int h = 0;
    int i = 0;
    for (;;) {
        i = s.indexOf(QLatin1Char(','), i);
        if (i >= 0) {
            if (i + 1 < s.length() && s.at(i + 1) == QLatin1Char(',')) {
                s.remove(i, 1);
                i++;
                continue;
            }
            lst << s.mid(h, i - h);
            i++;
            h = i;
        } else {
            lst << s.mid(h);
            break;
        }
    }
    return lst;
}

int main(int argc, char **argv)
{
    // Save arguments before QCoreApplication handles them
    QStringList args;
    for (int i = 1; i < argc; i++)
        args.append(argv[i]);

    QStringList defaultArgs;
    quint16 gdbDebugPort = 0;
    bool useGDB = false;
    bool useQML = false;
    QStringList perfParams;
    bool fireAndForget = false;
    bool detach = false;
    Utils::PortList range;

    if (args.isEmpty()) {
        fprintf(stderr, "No arguments given.\n");
        return 1;
    }

    Config config = parseConfigFile();

    while (!args.isEmpty()) {
        const QString arg(args.takeFirst());

        if (arg == "--port-range") {
            if (args.isEmpty()) {
                fprintf(stderr, "--port-range requires a range specification\n");
                return 1;
            }
            range = Utils::PortList::fromString(args.takeFirst());
            if (!range.hasMore()) {
                fprintf(stderr, "Invalid port range\n");
                return 1;
            }
        } else if (arg == "--debug-gdb") {
            useGDB = true;
            setpgid(0,0); // must be called before setsid()
            setsid();
        } else if (arg == "--debug-qml") {
            useQML = true;
        } else if (arg == "--profile-perf") {
            if (args.isEmpty()) {
                fprintf(stderr, "--profile-perf requires comma-separated list of parameters that "
                                "get passed to \"perf record\". Arguments \"-o -\" are "
                                "automatically appended to capture the output as stream. "
                                "Escape commas by doubling them.");
                return 1;
            }
            perfParams = extractPerfParams(args.takeFirst());
        } else if (arg == "--stop") {
            stop();
            return 0;
        } else if (arg == "--launch") {
            fireAndForget = true;
        } else if (arg == "--show-platform") {
            printf("base:%s\nplatform:%s\n",
                config.base.toLocal8Bit().constData(),
                config.platform.toLocal8Bit().constData());
            return 0;
        } else if (arg == "--make-default") {
              if (args.isEmpty()) {
                  fprintf(stderr, "--make-default requires an argument\n");
                  return 1;
              }
              if (!makeDefault(args.takeFirst()))
                  return 1;
              return 0;
        } else if (arg == "--remove-default") {
              if (removeDefault())
                  return 0;
              else
                  return 1;
        } else if (arg == "--print-debug") {
            config.flags |= Config::PrintDebugMessages;
        } else if (arg == "--version") {
            printf("Appcontroller version: " GIT_VERSION "\nGit revision: " GIT_HASH "\n");
            return 0;
        } else if (arg == "--detach") {
            detach = true;
        } else if (arg == "--help" || arg == "-help" || arg == "-h") {
            usage();
            return 0;
        } else {
            args.prepend(arg);
            break;
        }
    }

    if (args.isEmpty()) {
        fprintf(stderr, "No binary to execute.\n");
        return 1;
    }

    if ((useGDB || useQML) && !range.hasMore()) {
        fprintf(stderr, "--port-range is mandatory\n");
        return 1;
    }

    if (detach && (useGDB || useQML)) {
        fprintf(stderr, "Detached debugging not possible. --detach and one of --useGDB, --useQML must not be used together.\n");
        return 1;
    }

    if (useGDB) {
        int port = findFirstFreePort(range);
        if (port < 0) {
            fprintf(stderr, "Could not find an unused port in range\n");
            return 1;
        }
        gdbDebugPort = port;
    }
    if (useQML) {
        int port = findFirstFreePort(range);
        if (port < 0) {
            fprintf(stderr, "Could not find an unused port in range\n");
            return 1;
        }
        defaultArgs.push_front("-qmljsdebugger=port:" + QString::number(port) + ",block");
        printf("QML Debugger: Going to wait for connection on port %d...\n", port);
    }

    defaultArgs.push_front(args.takeFirst());
    defaultArgs.append(args);

    if (useGDB) {
        QString interface;
        if (config.debugInterface == Config::LocalDebugInterface)
            interface = QLatin1String("localhost");

        defaultArgs.push_front(interface + ":" + QString::number(gdbDebugPort));
        defaultArgs.push_front("gdbserver");
    }

    if (!fireAndForget && createServerSocket() != 0) {
        fprintf(stderr, "Could not create serversocket\n");
        return 1;
    }

    // daemonize
    if (detach) {
        pid_t rc = fork();
        if (rc == -1) {
            printf("fork failed\n");
            return -1;
        } else if (rc > 0) {
            // parent
            ::wait(NULL); // wait for the child to exit
            return 0;
        }

        setsid();
        chdir("/");
        signal(SIGHUP, SIG_IGN);

        // child
        int devnull = open("/dev/null", O_RDWR);
        if (devnull < 0)
            return -1;
        dup2(devnull, 0); // Replace file descriptors
        dup2(devnull, 1);
        dup2(devnull, 2);
        rc = fork();
        if (rc == -1)
            return -1;
        else if (rc > 0)
            return 0;

        // child
    }

    // Create QCoreApplication after parameter parsing to prevent printing evaluation
    // message to terminal before QtCreator has parsed the output.
    QCoreApplication app(argc, argv);
    Process process;
    process.setConfig(config);
    if (gdbDebugPort)
        process.setDebug();
    process.setSocketNotifier(new QSocketNotifier(serverSocket, QSocketNotifier::Read, &process));

    if (!perfParams.isEmpty()) {
        QStringList allArgs;
        allArgs << QLatin1String("perf") << QLatin1String("record")
                << perfParams << QLatin1String("-o") << QLatin1String("-")
                << QLatin1String("--") << defaultArgs.join(QLatin1Char(' '));

        PerfProcessHandler *server = new PerfProcessHandler(&process, allArgs);
        int port = openServer(server->server(), range);
        if (port < 0) {
            fprintf(stderr, "Could not find an unused port in range\n");
            return 1;
        }
        printf("AppController: Going to wait for perf connection on port %d...\n", port);
    } else {
        process.start(defaultArgs);
    }

    app.exec();
    if (!fireAndForget)
        close(serverSocket);
    return 0;
}

