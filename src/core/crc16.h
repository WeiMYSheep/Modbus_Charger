#ifndef CRC16_H
#define CRC16_H

#include <QByteArray>
#include <QtGlobal>

namespace Core {

quint16 crc16Modbus(const QByteArray &data);
QByteArray crcBytes(const QByteArray &data);
bool verifyCrc(const QByteArray &frame);
QString toHex(const QByteArray &data);

}

#endif

