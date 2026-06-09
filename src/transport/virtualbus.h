#ifndef VIRTUALBUS_H
#define VIRTUALBUS_H

#include <QByteArray>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>
#include <functional>

namespace Transport {

struct BusLogEntry {
    QDateTime timestamp;
    QString direction;
    QByteArray payload;
    QString format() const;
};

class Endpoint
{
public:
    using ExchangeFunction = std::function<QByteArray(const QByteArray &)>;

    explicit Endpoint(class VirtualBus *bus = nullptr);
    explicit Endpoint(ExchangeFunction exchange);
    QByteArray exchange(const QByteArray &payload);

private:
    ExchangeFunction m_exchange;
};

class VirtualBus : public QObject
{
    Q_OBJECT
public:
    using Handler = std::function<QByteArray(const QByteArray &)>;

    explicit VirtualBus(QObject *parent = nullptr);
    void setHandler(Handler handler);
    Endpoint endpoint();
    QByteArray exchange(const QByteArray &payload);
    void appendLog(const QString &direction, const QByteArray &payload);
    QVector<BusLogEntry> log() const;
    void clearLog();

signals:
    void frameLogged();

private:
    Handler m_handler;
    QVector<BusLogEntry> m_log;
};

}

#endif
