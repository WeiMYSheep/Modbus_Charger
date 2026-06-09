#include "crc16.h"

#include <QLatin1Char>
#include <QString>
#include <QStringList>

namespace Core {

quint16 crc16Modbus(const QByteArray &data)
{
    quint16 crc = 0xFFFF;
    for (unsigned char byte : data) {
        crc ^= byte;
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x0001) {
                crc = static_cast<quint16>((crc >> 1) ^ 0xA001);
            } else {
                crc = static_cast<quint16>(crc >> 1);
            }
        }
    }
    return crc;
}

QByteArray crcBytes(const QByteArray &data)
{
    const quint16 crc = crc16Modbus(data);
    QByteArray bytes;
    bytes.append(static_cast<char>(crc & 0xFF));
    bytes.append(static_cast<char>((crc >> 8) & 0xFF));
    return bytes;
}

bool verifyCrc(const QByteArray &frame)
{
    if (frame.size() < 4) {
        return false;
    }
    const QByteArray payload = frame.left(frame.size() - 2);
    return crcBytes(payload) == frame.right(2);
}

QString toHex(const QByteArray &data)
{
    QStringList parts;
    for (unsigned char byte : data) {
        parts << QString("%1").arg(byte, 2, 16, QLatin1Char('0')).toUpper();
    }
    return parts.join(' ');
}

}
