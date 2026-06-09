#include "modbuslabservice.h"

#include "core/crc16.h"
#include "core/modbusframe.h"
#include "core/registermap.h"

#include <QStringList>

namespace Services {

QString LabFrame::hex() const
{
    return Core::toHex(raw);
}

QString LabFrame::explanation() const
{
    return Core::explainFrame(raw);
}

LabFrame ModbusLabService::buildReadAllParameters(quint8 address) const
{
    return {"读取全部充电参数", "读取电压、电流、门限、温度、电量和容量", Core::ModbusFrame::buildReadRegisters(address, Core::Voltage, 8)};
}

LabFrame ModbusLabService::buildReadCard(quint8 address) const
{
    return {"读取卡号", "读取0008-000A三个BCD卡号寄存器", Core::ModbusFrame::buildReadRegisters(address, Core::CardId14, 3)};
}

LabFrame ModbusLabService::buildStart(quint8 address) const
{
    return {"启动充电", "写控制线圈为FF00", Core::ModbusFrame::buildWriteCoil(address, Core::ChargeControlCoil, true)};
}

LabFrame ModbusLabService::buildStop(quint8 address) const
{
    return {"停止充电", "写控制线圈为0000", Core::ModbusFrame::buildWriteCoil(address, Core::ChargeControlCoil, false)};
}

LabFrame ModbusLabService::buildWriteCard(const QVector<quint16> &registers, quint8 address) const
{
    return {"写入卡号", "将10位学号按BCD写入0008-000A", Core::ModbusFrame::buildWriteRegisters(address, Core::CardId14, registers)};
}

LabFrame ModbusLabService::buildInvalidRegisterDemo(quint8 address) const
{
    return {"非法寄存器示例", "读取000B起2个寄存器，用于触发84 02", Core::ModbusFrame::buildReadRegisters(address, 0x000B, 2)};
}

QVector<LabFrame> ModbusLabService::examples(quint8 address) const
{
    return {
        buildStart(address),
        buildWriteCard(Core::cardIdToBcdRegisters("2428403001"), address),
        buildReadAllParameters(address),
        buildReadCard(address),
        buildInvalidRegisterDemo(address),
        buildStop(address)
    };
}

QString ModbusLabService::examplesText(quint8 address) const
{
    QStringList blocks;
    for (const LabFrame &frame : examples(address)) {
        blocks << QString("%1 - %2\n%3\n%4").arg(frame.title, frame.description, frame.hex(), frame.explanation());
    }
    return blocks.join("\n\n");
}

QString ModbusLabService::compareFrames(const QByteArray &expected, const QByteArray &actual) const
{
    QStringList messages;
    const int maxSize = qMax(expected.size(), actual.size());
    for (int i = 0; i < maxSize; ++i) {
        const int exp = i < expected.size() ? static_cast<unsigned char>(expected.at(i)) : -1;
        const int act = i < actual.size() ? static_cast<unsigned char>(actual.at(i)) : -1;
        if (exp != act) {
            if (exp < 0) {
                messages << QString("第%1字节多余: %2").arg(i + 1).arg(act, 2, 16, QLatin1Char('0')).toUpper();
            } else if (act < 0) {
                messages << QString("第%1字节缺失，期望: %2").arg(i + 1).arg(exp, 2, 16, QLatin1Char('0')).toUpper();
            } else {
                messages << QString("第%1字节不同: 实际%2，期望%3")
                                .arg(i + 1)
                                .arg(act, 2, 16, QLatin1Char('0'))
                                .arg(exp, 2, 16, QLatin1Char('0'))
                                .toUpper();
            }
        }
    }
    return messages.isEmpty() ? "完全一致" : messages.join('\n');
}

}

