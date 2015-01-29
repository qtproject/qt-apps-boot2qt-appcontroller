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
