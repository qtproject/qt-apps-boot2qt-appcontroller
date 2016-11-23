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

#ifndef PORTLIST_H
#define PORTLIST_H

//#include "utils_global.h"

class QString;

namespace Utils {
namespace Internal {
class PortListPrivate;
} // namespace Internal

class PortList
{
public:
    PortList();
    PortList(const PortList &other);
    PortList &operator=(const PortList &other);
    ~PortList();

    void addPort(int port);
    void addRange(int startPort, int endPort);
    bool hasMore() const;
    bool contains(int port) const;
    int count() const;
    int getNext();
    QString toString() const;

    static PortList fromString(const QString &portsSpec);
    static QString regularExpression();

private:
    Internal::PortListPrivate * const d;
};

} // namespace Utils

#endif // PORTLIST_H
