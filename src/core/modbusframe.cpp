#include "modbusframe.h"

#include "crc16.h"

#include <QStringList>
#include <QVector>

namespace Core {

ProtocolException::ProtocolException(QString message)
    : m_message(std::move(message))
{
}

QString ProtocolException::message() const
{
    return m_message;
}

ModbusFrame::ModbusFrame() = default;

ModbusFrame::ModbusFrame(quint8 address, quint8 function, QByteArray data)
    : m_address(address), m_function(function), m_data(std::move(data))
{
}

quint8 ModbusFrame::address() const
{
    return m_address;
}

quint8 ModbusFrame::function() const
{
    return m_function;
}

QByteArray ModbusFrame::data() const
{
    return m_data;
}

bool ModbusFrame::isException() const
{
    return (m_function & 0x80) != 0;
}

QByteArray ModbusFrame::toBytes() const
{
    QByteArray payload;
    payload.append(static_cast<char>(m_address));
    payload.append(static_cast<char>(m_function));
    payload.append(m_data);
    payload.append(crcBytes(payload));
    return payload;
}

ModbusFrame ModbusFrame::parse(const QByteArray &raw)
{
    if (raw.size() < 4) {
        throw ProtocolException("帧长度不足");
    }
    if (!verifyCrc(raw)) {
        throw ProtocolException("CRC 校验失败");
    }
    return ModbusFrame(static_cast<quint8>(raw.at(0)),
                       static_cast<quint8>(raw.at(1)),
                       raw.mid(2, raw.size() - 4));
}

QByteArray ModbusFrame::buildReadRegisters(quint8 address, quint16 start, quint16 quantity)
{
    return ModbusFrame(address, ReadInputRegisters, writeU16(start) + writeU16(quantity)).toBytes();
}

QByteArray ModbusFrame::buildWriteCoil(quint8 address, quint16 coil, bool enabled)
{
    QByteArray data = writeU16(coil);
    data.append(enabled ? QByteArray::fromHex("FF00") : QByteArray::fromHex("0000"));
    return ModbusFrame(address, WriteSingleCoil, data).toBytes();
}

QByteArray ModbusFrame::buildWriteRegisters(quint8 address, quint16 start, const QVector<quint16> &values)
{
    QByteArray registers;
    for (quint16 value : values) {
        registers.append(writeU16(value));
    }
    QByteArray data = writeU16(start) + writeU16(static_cast<quint16>(values.size()));
    data.append(static_cast<char>(registers.size()));
    data.append(registers);
    return ModbusFrame(address, WriteMultipleRegisters, data).toBytes();
}

QByteArray ModbusFrame::buildException(quint8 address, quint8 function, ExceptionCode code)
{
    QByteArray data;
    data.append(static_cast<char>(code));
    return ModbusFrame(address, static_cast<quint8>(function | 0x80), data).toBytes();
}

quint16 readU16(const QByteArray &bytes, int offset)
{
    return static_cast<quint16>((static_cast<unsigned char>(bytes.at(offset)) << 8) |
                                static_cast<unsigned char>(bytes.at(offset + 1)));
}

QByteArray writeU16(quint16 value)
{
    QByteArray bytes;
    bytes.append(static_cast<char>((value >> 8) & 0xFF));
    bytes.append(static_cast<char>(value & 0xFF));
    return bytes;
}

QString explainFrame(const QByteArray &raw)
{
    QStringList lines;
    lines << QString("原始帧: %1").arg(toHex(raw));
    if (raw.size() < 4) {
        lines << "说明: 帧长度不足。";
        return lines.join('\n');
    }
    lines << QString("地址: %1").arg(static_cast<unsigned char>(raw.at(0)));
    const quint8 function = static_cast<quint8>(raw.at(1));
    lines << QString("功能码: 0x%1").arg(function, 2, 16, QLatin1Char('0')).toUpper();
    lines << QString("数据区: %1").arg(toHex(raw.mid(2, raw.size() - 4)));
    lines << QString("CRC: %1 (%2)").arg(toHex(raw.right(2)), verifyCrc(raw) ? "正确" : "错误");
    if (function == ReadInputRegisters && raw.size() >= 8) {
        lines << QString("说明: 读取起始寄存器 0x%1，数量 %2。")
                     .arg(readU16(raw, 2), 4, 16, QLatin1Char('0')).toUpper()
                     .arg(readU16(raw, 4));
    } else if (function == WriteSingleCoil && raw.size() >= 8) {
        lines << QString("说明: 写线圈 0x%1，值 0x%2。")
                     .arg(readU16(raw, 2), 4, 16, QLatin1Char('0')).toUpper()
                     .arg(readU16(raw, 4), 4, 16, QLatin1Char('0')).toUpper();
    } else if (function & 0x80) {
        lines << QString("说明: 异常响应，异常码 %1。").arg(static_cast<unsigned char>(raw.at(2)));
    }
    return lines.join('\n');
}

}

