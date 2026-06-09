#include "virtualbus.h"

#include "core/crc16.h"

namespace Transport {

QString BusLogEntry::format() const
{
    return QString("%1 %-2 %2")
        .arg(timestamp.toString("HH:mm:ss.zzz"))
        .arg(direction)
        .arg(Core::toHex(payload));
}

Endpoint::Endpoint(VirtualBus *bus)
    : m_exchange([bus](const QByteArray &payload) {
        return bus ? bus->exchange(payload) : QByteArray();
    })
{
}

Endpoint::Endpoint(ExchangeFunction exchange)
    : m_exchange(std::move(exchange))
{
}

QByteArray Endpoint::exchange(const QByteArray &payload)
{
    return m_exchange ? m_exchange(payload) : QByteArray();
}

VirtualBus::VirtualBus(QObject *parent)
    : QObject(parent)
{
}

void VirtualBus::setHandler(Handler handler)
{
    m_handler = std::move(handler);
}

Endpoint VirtualBus::endpoint()
{
    return Endpoint(this);
}

QByteArray VirtualBus::exchange(const QByteArray &payload)
{
    appendLog("TX", payload);
    QByteArray response;
    if (m_handler) {
        response = m_handler(payload);
    }
    if (!response.isEmpty()) {
        appendLog("RX", response);
    }
    return response;
}

void VirtualBus::appendLog(const QString &direction, const QByteArray &payload)
{
    m_log.append({QDateTime::currentDateTime(), direction, payload});
    emit frameLogged();
}

QVector<BusLogEntry> VirtualBus::log() const
{
    return m_log;
}

void VirtualBus::clearLog()
{
    m_log.clear();
    emit frameLogged();
}

}
