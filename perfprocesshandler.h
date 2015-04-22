/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd
** All rights reserved.
** For any questions to The Qt Company, please use contact form at http://www.qt.io/contact-us
**
** This file is part of QtEnterprise Embedded.
**
** Licensees holding valid Qt Enterprise licenses may use this file in
** accordance with the Qt Enterprise License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company.
**
** If you have questions regarding the use of this file, please use
** contact form at http://www.qt.io/contact-us
**
****************************************************************************/

#ifndef PERFPROCESSHANDLER_H
#define PERFPROCESSHANDLER_H

#include "process.h"
#include <QTcpServer>

// Starts the process once a connection to the TCP server is established and then deletes itself.
class PerfProcessHandler : public QObject {
    Q_OBJECT

private:
    QTcpServer mServer;
    Process *mProcess;
    QStringList mAllArgs;

public:
    PerfProcessHandler(Process *process, const QStringList &allArgs);
    QTcpServer *server();

public slots:
    void acceptConnection();
};

#endif // PERFPROCESSHANDLER_H
