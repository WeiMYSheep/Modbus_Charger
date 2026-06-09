#ifndef MODBUSFRAME_H
#define MODBUSFRAME_H

#include <QByteArray>
#include <QString>
#include <QtGlobal>

namespace Core {

enum FunctionCode : quint8 {
    ReadInputRegisters = 0x04,
    WriteSingleCoil = 0x05,
    WriteMultipleRegisters = 0x16
};

enum ExceptionCode : quint8 {
    IllegalFunction = 0x01,
    IllegalDataAddress = 0x02,
    IllegalDataValue = 0x03,
    ServerFailure = 0x04
};

class ProtocolException
{
public:
    explicit ProtocolException(QString message);
    QString message() const;

private:
    QString m_message;
};

class ModbusFrame
{
public:
    ModbusFrame();
    ModbusFrame(quint8 address, quint8 function, QByteArray data);

    quint8 address() const;
    quint8 function() const;
    QByteArray data() const;
    bool isException() const;
    QByteArray toBytes() const;

    static ModbusFrame parse(const QByteArray &raw);
    static QByteArray buildReadRegisters(quint8 address, quint16 start, quint16 quantity);
    static QByteArray buildWriteCoil(quint8 address, quint16 coil, bool enabled);
    static QByteArray buildWriteRegisters(quint8 address, quint16 start, const QVector<quint16> &values);
    static QByteArray buildException(quint8 address, quint8 function, ExceptionCode code);

private:
    quint8 m_address = 0;
    quint8 m_function = 0;
    QByteArray m_data;
};

quint16 readU16(const QByteArray &bytes, int offset);
QByteArray writeU16(quint16 value);
QString explainFrame(const QByteArray &raw);

}

#endif

