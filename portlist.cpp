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

#include "portlist.h"

#include <QList>
#include <QPair>
#include <QString>

#include <cctype>

namespace Utils {
namespace Internal {
namespace {

typedef QPair<int, int> Range;

class PortsSpecParser
{
    struct ParseException {
        ParseException(const char *error) : error(error) {}
        const char * const error;
    };

public:
    PortsSpecParser(const QString &portsSpec)
        : m_pos(0), m_portsSpec(portsSpec) { }

    /*
     * Grammar: Spec -> [ ElemList ]
     *          ElemList -> Elem [ ',' ElemList ]
     *          Elem -> Port [ '-' Port ]
     */
    bool parse()
    {
        if (!atEnd()) {
            if (!parseElemList()) {
                qWarning("Malformed ports specification");
                return false;
            }
        }
        return true;
    }

    const PortList &portList() const
    {
        return m_portList;
    }

private:
    bool parseElemList()
    {
        if (atEnd()) {
            qWarning("Element list empty.");
            return false;
        }
        if (!parseElem())
            return false;
        if (atEnd())
            return true;
        if (nextChar() != ',') {
            qWarning("Element followed by something else than a comma.");
            return false;
        }
        ++m_pos;
        return parseElemList();
    }

    bool parseElem()
    {
        const int startPort = parsePort();
        if (startPort < 0)
            return false;

        if (atEnd() || nextChar() != '-') {
            m_portList.addPort(startPort);
            return true;
        }
        ++m_pos;
        const int endPort = parsePort();
        if (endPort < 0)
            return false;

        if (endPort < startPort) {
            qWarning("Invalid range (end < start).");
            return false;
        }
        m_portList.addRange(startPort, endPort);
        return true;
    }

    int parsePort()
    {
        if (atEnd()) {
            qWarning("Empty port string.");
            return -1;
        }
        int port = 0;
        do {
            const char next = nextChar();
            if (!std::isdigit(next))
                break;
            port = 10*port + next - '0';
            ++m_pos;
        } while (!atEnd());
        if (port == 0 || port >= 2 << 16) {
            qWarning("Invalid port value.");
            return -1;
        }
        return port;
    }

    bool atEnd() const { return m_pos == m_portsSpec.length(); }
    char nextChar() const { return m_portsSpec.at(m_pos).toLatin1(); }

    PortList m_portList;
    int m_pos;
    const QString &m_portsSpec;
};

} // anonymous namespace

class PortListPrivate
{
public:
    QList<Range> ranges;
};

} // namespace Internal

PortList::PortList() : d(new Internal::PortListPrivate)
{
}

PortList::PortList(const PortList &other) : d(new Internal::PortListPrivate(*other.d))
{
}

PortList::~PortList()
{
    delete d;
}

PortList &PortList::operator=(const PortList &other)
{
    *d = *other.d;
    return *this;
}

PortList PortList::fromString(const QString &portsSpec)
{
    Internal::PortsSpecParser p(portsSpec);
    if (!p.parse()) {
        qWarning("Could not parse string");
    }
    return p.portList();
}

void PortList::addPort(int port) { addRange(port, port); }

void PortList::addRange(int startPort, int endPort)
{
    d->ranges << Internal::Range(startPort, endPort);
}

bool PortList::hasMore() const { return !d->ranges.isEmpty(); }

bool PortList::contains(int port) const
{
    foreach (const Internal::Range &r, d->ranges) {
        if (port >= r.first && port <= r.second)
            return true;
    }
    return false;
}

int PortList::count() const
{
    int n = 0;
    foreach (const Internal::Range &r, d->ranges)
        n += r.second - r.first + 1;
    return n;
}

int PortList::getNext()
{
    Q_ASSERT(!d->ranges.isEmpty());

    Internal::Range &firstRange = d->ranges.first();
    const int next = firstRange.first++;
    if (firstRange.first > firstRange.second)
        d->ranges.removeFirst();
    return next;
}

QString PortList::toString() const
{
    QString stringRep;
    foreach (const Internal::Range &range, d->ranges) {
        stringRep += QString::number(range.first);
        if (range.second != range.first)
            stringRep += QLatin1Char('-') + QString::number(range.second);
        stringRep += QLatin1Char(',');
    }
    if (!stringRep.isEmpty())
        stringRep.remove(stringRep.length() - 1, 1); // Trailing comma.
    return stringRep;
}

QString PortList::regularExpression()
{
    const QLatin1String portExpr("(\\d)+");
    const QString listElemExpr = QString::fromLatin1("%1(-%1)?").arg(portExpr);
    return QString::fromLatin1("((%1)(,%1)*)?").arg(listElemExpr);
}

} // namespace Utils
