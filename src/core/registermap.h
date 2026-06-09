#ifndef REGISTERMAP_H
#define REGISTERMAP_H

#include <QString>
#include <QVector>
#include <QtGlobal>

namespace Core {

enum RegisterAddress : quint16 {
    Voltage = 0x0000,
    Current = 0x0001,
    VoltageLimit = 0x0002,
    CurrentLimit = 0x0003,
    Temperature = 0x0004,
    TemperatureLimit = 0x0005,
    BatteryPower = 0x0006,
    BatteryPowerLimit = 0x0007,
    CardId14 = 0x0008,
    CardId58 = 0x0009,
    CardId910 = 0x000A
};

constexpr quint16 ChargeControlCoil = 0x0000;

QString registerName(quint16 address);
bool isDefinedRegister(quint16 address);
bool isDefinedRange(quint16 start, quint16 quantity);
QString normalizeCardId(const QString &cardId);
QVector<quint16> cardIdToBcdRegisters(const QString &cardId);
QString bcdRegistersToCardId(const QVector<quint16> &registers);

}

#endif

