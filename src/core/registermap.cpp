#include "registermap.h"

#include <QMap>

namespace Core {

QString registerName(quint16 address)
{
    static const QMap<quint16, QString> names = {
        {Voltage, "充电电压(V)"},
        {Current, "充电电流(A)"},
        {VoltageLimit, "电压上限(V)"},
        {CurrentLimit, "电流上限(A)"},
        {Temperature, "当前温度(°C)"},
        {TemperatureLimit, "温度上限(°C)"},
        {BatteryPower, "电池电量(kWh)"},
        {BatteryPowerLimit, "最大电量(kWh)"},
        {CardId14, "卡号1-4"},
        {CardId58, "卡号5-8"},
        {CardId910, "卡号9-10"}
    };
    return names.value(address, "未定义寄存器");
}

bool isDefinedRegister(quint16 address)
{
    return address <= CardId910;
}

bool isDefinedRange(quint16 start, quint16 quantity)
{
    if (quantity == 0) {
        return false;
    }
    for (quint16 offset = 0; offset < quantity; ++offset) {
        if (!isDefinedRegister(static_cast<quint16>(start + offset))) {
            return false;
        }
    }
    return true;
}

QString normalizeCardId(const QString &cardId)
{
    QString digits;
    for (QChar ch : cardId) {
        if (ch.isDigit()) {
            digits.append(ch);
        }
    }
    while (digits.size() < 10) {
        digits.prepend('0');
    }
    if (digits.size() > 10) {
        digits = digits.right(10);
    }
    return digits;
}

QVector<quint16> cardIdToBcdRegisters(const QString &cardId)
{
    const QString digits = normalizeCardId(cardId);
    QVector<quint8> bcd;
    for (int i = 0; i < 10; i += 2) {
        const quint8 high = static_cast<quint8>(digits.mid(i, 1).toInt());
        const quint8 low = static_cast<quint8>(digits.mid(i + 1, 1).toInt());
        bcd.append(static_cast<quint8>((high << 4) | low));
    }
    bcd.append(0);
    return {
        static_cast<quint16>((bcd[0] << 8) | bcd[1]),
        static_cast<quint16>((bcd[2] << 8) | bcd[3]),
        static_cast<quint16>((bcd[4] << 8) | bcd[5])
    };
}

QString bcdRegistersToCardId(const QVector<quint16> &registers)
{
    QVector<quint8> bytes;
    for (int i = 0; i < qMin(3, registers.size()); ++i) {
        bytes.append(static_cast<quint8>((registers[i] >> 8) & 0xFF));
        bytes.append(static_cast<quint8>(registers[i] & 0xFF));
    }
    QString digits;
    for (int i = 0; i < qMin(5, bytes.size()); ++i) {
        const int high = (bytes[i] >> 4) & 0x0F;
        const int low = bytes[i] & 0x0F;
        digits.append(QString::number(high));
        digits.append(QString::number(low));
    }
    return digits;
}

}

