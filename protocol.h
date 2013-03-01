#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <QObject>
#include <QStringList>

class Protocol : public QObject
{
Q_OBJECT

public:
    Protocol(QObject *parent = 0);
    virtual ~Protocol();

    void write(const QByteArray &);
    QString sendToClient(const QStringList &);

signals:
    void commandReceived(const QStringList &);

private:
    void reset();
    enum State { Start, Field };
    State mState;
    QStringList mFields;
    QString mField;
};

#endif // PROTOCOL_H
