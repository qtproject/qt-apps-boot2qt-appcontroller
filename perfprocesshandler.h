/******************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of Qt for Device Creation.
**
** $QT_BEGIN_LICENSE:COMM$
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** $QT_END_LICENSE$
**
******************************************************************************/

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
