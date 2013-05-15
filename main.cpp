#include "process.h"
#include "portlist.h"
#include <QCoreApplication>
#include <QTcpServer>
#include <QProcess>
#include <errno.h>
#include <QStringList>
#include <QSocketNotifier>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>

#define PID_FILE "/data/user/.appcontroller"

static int serverSocket = -1;

static const char socketPath[] = "#Boot2Qt_appcontroller";

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

static int findFirstFreePort(Utils::PortList range)
{
    QTcpServer s;

    while (range.hasMore()) {
        if (s.listen(QHostAddress::Any, range.getNext()))
            return s.serverPort();
    }
    return -1;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QStringList defaultArgs;
    QString binary;
    bool debug = false;

    QStringList args = app.arguments();
    args.removeFirst();
    if (args.size() == 0) {
        qWarning("No arguments given.");
        return 1;
    }

    while (!args.isEmpty()) {
        if (args[0] == "--start") {
            if (args.size() < 2) {
                qWarning("--start requires an argument");
                return 1;
            }
            binary = args[1];
            args.removeFirst();
            if (binary.isEmpty()) {
                qWarning("App path is empty");
                return 1;
            }
            defaultArgs.append(args);
            break;
        } else if (args[0] == "--debug") {
            debug = true;
            if (args.size() < 3) {
                qWarning("--debug requires arguments: port-range and executable");
                return 1;
            }
            Utils::PortList range = Utils::PortList::fromString(args[1]);
            binary = args[2];
            args.removeFirst();
            args.removeFirst();
            if (binary.isEmpty()) {
                qWarning("App path is empty");
                return 1;
            }

            int port = findFirstFreePort(range);
            if (port < 0) {
                qWarning("Could not find an unused port in range");
                return 1;
            }
            defaultArgs.push_front("localhost:" + QString::number(port));
            defaultArgs.push_front("gdbserver");
            defaultArgs.append(args);
            setpgid(0,0); // must be called before setsid()
            setsid();
            break;
        } else if (args[0] == "--stop") {
            stop();
            return 0;
        } else {
            qWarning("unknown argument: %s", args.first().toLocal8Bit().constData());
            return 1;
        }
        args.removeFirst();
    }

    if (createServerSocket() != 0) {
        fprintf(stderr, "Could not create serversocket\n");
        return 1;
    }

    Process process;
    if (debug)
        process.setDebug();
    process.setSocketNotifier(new QSocketNotifier(serverSocket, QSocketNotifier::Read, &process));
    process.start(defaultArgs);
    app.exec();
    close(serverSocket);
    return 0;
}

