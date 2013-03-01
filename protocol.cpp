#include "protocol.h"
#include <QDebug>

Protocol::Protocol(QObject *parent)
    : QObject(parent)
    , mState(Start)
{
}

Protocol::~Protocol()
{
}

void Protocol::reset()
{
    mState = Start;
    mFields.clear();
    mField.clear();
}

void Protocol::write(const QByteArray &data)
{
    QByteArray buffer(data);

    while (!buffer.isEmpty()) {
        unsigned int remove = 0;

        if (buffer[0] == '@') {
            if (buffer.size() > 1) {
                if (buffer[1] == '@' && mState != Start) {
                    mField += '@';
                    remove += 2;
                } else if (buffer[1] == '\n' || buffer[1] == '\r') {
                    remove += 2;
                    if (mState != Field) {
                        reset();
                        qDebug() << "End unexpected";
                    } else {
                        mFields += mField;
                        emit commandReceived(mFields);
                        reset();
                    }
                } else {
                    ++remove;
                    if (mState != Start) {
                       qDebug() << "Start unexpected";
                    } else {
                        mState = Field;
                        mField.clear();
                        mFields.clear();
                    }
                }
            }
        } else if (buffer[0] == ':') {
            if (buffer.size() > 1) {
                if (buffer[1] == ':') {
                    mField += ':';
                    remove += 2;
                } else {
                    ++remove;
                    mFields += mField;
                    mField.clear();
                }
            }
        } else {
            ++remove;
            mField += buffer[0];
        }

        buffer.remove(0, remove);
    }
}

QString Protocol::sendToClient(const QStringList &data)
{
    bool first = true;
    QString rc("@");
    foreach (QString s, data) {
        s.replace(":", "::");
        s.replace("@", "@@");
        if (!first)
            rc += ":";
        else
            first = false;

        rc += s;
    }
    rc += "@\n";
    return rc;
}
