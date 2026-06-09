#include "collectorservice.h"

#include "core/registermap.h"

namespace Services {

CollectorService::CollectorService(quint8 address, QObject *parent)
    : QObject(parent), m_address(address)
{
}

QByteArray CollectorService::handleRequest(const QByteArray &rawFrame)
{
    Core::ModbusFrame frame;
    try {
        frame = Core::ModbusFrame::parse(rawFrame);
    } catch (const Core::ProtocolException &) {
        return Core::ModbusFrame::buildException(m_address, 0, Core::ServerFailure);
    }
    if (frame.address() != m_address) {
        return {};
    }

    m_simulator.tick();
    switch (frame.function()) {
    case Core::ReadInputRegisters:
        return readRegisters(frame);
    case Core::WriteSingleCoil:
        return writeSingleCoil(frame);
    case Core::WriteMultipleRegisters:
        return writeMultipleRegisters(frame);
    default:
        return Core::ModbusFrame::buildException(m_address, frame.function(), Core::IllegalFunction);
    }
}

Core::BatterySimulator &CollectorService::simulator()
{
    return m_simulator;
}

const Core::BatterySimulator &CollectorService::simulator() const
{
    return m_simulator;
}

quint8 CollectorService::address() const
{
    return m_address;
}

void CollectorService::setAddress(quint8 address)
{
    m_address = address;
}

QByteArray CollectorService::readRegisters(const Core::ModbusFrame &frame)
{
    if (frame.data().size() != 4) {
        return Core::ModbusFrame::buildException(m_address, frame.function(), Core::IllegalDataValue);
    }
    const quint16 start = Core::readU16(frame.data(), 0);
    const quint16 quantity = Core::readU16(frame.data(), 2);
    if (!Core::isDefinedRange(start, quantity)) {
        return Core::ModbusFrame::buildException(m_address, frame.function(), Core::IllegalDataAddress);
    }
    QByteArray data;
    data.append(static_cast<char>(quantity * 2));
    for (quint16 i = 0; i < quantity; ++i) {
        data.append(Core::writeU16(registerValue(static_cast<quint16>(start + i))));
    }
    return Core::ModbusFrame(m_address, frame.function(), data).toBytes();
}

QByteArray CollectorService::writeSingleCoil(const Core::ModbusFrame &frame)
{
    if (frame.data().size() != 4) {
        return Core::ModbusFrame::buildException(m_address, frame.function(), Core::IllegalDataValue);
    }
    const quint16 coil = Core::readU16(frame.data(), 0);
    const quint16 value = Core::readU16(frame.data(), 2);
    if (coil != Core::ChargeControlCoil) {
        return Core::ModbusFrame::buildException(m_address, frame.function(), Core::IllegalDataAddress);
    }
    if (value == 0xFF00) {
        m_simulator.start();
    } else if (value == 0x0000) {
        m_simulator.stop();
    } else {
        return Core::ModbusFrame::buildException(m_address, frame.function(), Core::IllegalDataValue);
    }
    emit stateChanged();
    return Core::ModbusFrame(m_address, frame.function(), frame.data()).toBytes();
}

QByteArray CollectorService::writeMultipleRegisters(const Core::ModbusFrame &frame)
{
    const QByteArray data = frame.data();
    if (data.size() < 5) {
        return Core::ModbusFrame::buildException(m_address, frame.function(), Core::IllegalDataValue);
    }
    const quint16 start = Core::readU16(data, 0);
    const quint16 quantity = Core::readU16(data, 2);
    const quint8 byteCount = static_cast<quint8>(data.at(4));
    if (byteCount != quantity * 2 || data.mid(5).size() != byteCount) {
        return Core::ModbusFrame::buildException(m_address, frame.function(), Core::IllegalDataValue);
    }
    if (!Core::isDefinedRange(start, quantity)) {
        return Core::ModbusFrame::buildException(m_address, frame.function(), Core::IllegalDataAddress);
    }
    for (quint16 i = 0; i < quantity; ++i) {
        if (!setRegisterValue(static_cast<quint16>(start + i), Core::readU16(data, 5 + i * 2))) {
            return Core::ModbusFrame::buildException(m_address, frame.function(), Core::IllegalDataValue);
        }
    }
    emit stateChanged();
    return Core::ModbusFrame(m_address, frame.function(), data.left(4)).toBytes();
}

quint16 CollectorService::registerValue(quint16 address)
{
    m_simulator.tick();
    const Core::ChargingParameters &p = m_simulator.parameters();
    switch (address) {
    case Core::Voltage:
        return static_cast<quint16>(p.voltage);
    case Core::Current:
        return static_cast<quint16>(p.current);
    case Core::VoltageLimit:
        return static_cast<quint16>(p.voltageLimit);
    case Core::CurrentLimit:
        return static_cast<quint16>(p.currentLimit);
    case Core::Temperature:
        return static_cast<quint16>(p.temperature);
    case Core::TemperatureLimit:
        return static_cast<quint16>(p.temperatureLimit);
    case Core::BatteryPower:
        return static_cast<quint16>(p.batteryPower);
    case Core::BatteryPowerLimit:
        return static_cast<quint16>(p.batteryPowerLimit);
    case Core::CardId14:
    case Core::CardId58:
    case Core::CardId910: {
        const QVector<quint16> regs = Core::cardIdToBcdRegisters(m_simulator.cardId());
        return regs[address - Core::CardId14];
    }
    default:
        return 0;
    }
}

bool CollectorService::setRegisterValue(quint16 address, quint16 value)
{
    Core::ChargingParameters &p = m_simulator.parameters();
    switch (address) {
    case Core::Voltage:
        p.voltage = value;
        return true;
    case Core::Current:
        p.current = value;
        return true;
    case Core::VoltageLimit:
        p.voltageLimit = qMax<quint16>(1, value);
        return true;
    case Core::CurrentLimit:
        p.currentLimit = qMax<quint16>(1, value);
        return true;
    case Core::Temperature:
        p.temperature = value;
        return true;
    case Core::TemperatureLimit:
        p.temperatureLimit = qMax<quint16>(1, value);
        return true;
    case Core::BatteryPower:
        p.batteryPower = value;
        p.initialPower = value;
        return true;
    case Core::BatteryPowerLimit:
        p.batteryPowerLimit = qMax<quint16>(1, value);
        return true;
    case Core::CardId14:
    case Core::CardId58:
    case Core::CardId910: {
        QVector<quint16> regs = Core::cardIdToBcdRegisters(m_simulator.cardId());
        regs[address - Core::CardId14] = value;
        m_simulator.acceptCard(Core::bcdRegistersToCardId(regs));
        return true;
    }
    default:
        return false;
    }
}

}

