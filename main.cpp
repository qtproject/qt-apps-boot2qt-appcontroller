#include <QCoreApplication>
#include "daemon.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    Daemon daemon;
    return app.exec();
}
