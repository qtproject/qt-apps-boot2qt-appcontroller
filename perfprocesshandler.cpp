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

#include "perfprocesshandler.h"
#include <QTcpSocket>

PerfProcessHandler::PerfProcessHandler(Process *process, const QStringList &allArgs) : mProcess(process), mAllArgs(allArgs)
{
    QObject::connect(&mServer, &QTcpServer::newConnection, this, &PerfProcessHandler::acceptConnection);
}

QTcpServer *PerfProcessHandler::server()
{
    return &mServer;
}

void PerfProcessHandler::acceptConnection()
{
    QTcpSocket *socket = mServer.nextPendingConnection();
    socket->setParent(mProcess);
    mProcess->setStdoutFd(socket->socketDescriptor());
    mProcess->start(mAllArgs);
    this->deleteLater();
}
