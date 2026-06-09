#ifndef COLLECTORSERVICE_H
#define COLLECTORSERVICE_H

#include "core/batterysimulator.h"
#include "core/modbusframe.h"

#include <QByteArray>
#include <QObject>

namespace Services {

class CollectorService : public QObject
{
    Q_OBJECT
public:
    explicit CollectorService(quint8 address = 1, QObject *parent = nullptr);

    QByteArray handleRequest(const QByteArray &rawFrame);
    Core::BatterySimulator &simulator();
    const Core::BatterySimulator &simulator() const;
    quint8 address() const;
    void setAddress(quint8 address);

signals:
    void stateChanged();

private:
    QByteArray readRegisters(const Core::ModbusFrame &frame);
    QByteArray writeSingleCoil(const Core::ModbusFrame &frame);
    QByteArray writeMultipleRegisters(const Core::ModbusFrame &frame);
    quint16 registerValue(quint16 address);
    bool setRegisterValue(quint16 address, quint16 value);

    quint8 m_address;
    Core::BatterySimulator m_simulator;
};

}

#endif

