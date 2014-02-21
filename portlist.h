/****************************************************************************
**
** Copyright (C) 2014 Digia Plc
** All rights reserved.
** For any questions to Digia, please use contact form at http://qt.digia.com
**
** This file is part of QtEnterprise Embedded.
**
** Licensees holding valid Qt Enterprise licenses may use this file in
** accordance with the Qt Enterprise License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.
**
** If you have questions regarding the use of this file, please use
** contact form at http://qt.digia.com
**
****************************************************************************/

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
