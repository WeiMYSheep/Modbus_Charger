#include "batterysimulator.h"

#include <QtMath>
#include <QStringList>

namespace Core {

bool AlarmFlags::any() const
{
    return overVoltage || overCurrent || overTemperature;
}

QString AlarmFlags::text() const
{
    QStringList items;
    if (overVoltage) {
        items << "过压";
    }
    if (overCurrent) {
        items << "过流";
    }
    if (overTemperature) {
        items << "高温";
    }
    return items.isEmpty() ? "正常" : items.join("、");
}

ChargingParameters &BatterySimulator::parameters()
{
    return m_parameters;
}

const ChargingParameters &BatterySimulator::parameters() const
{
    return m_parameters;
}

ChargeState BatterySimulator::state() const
{
    return m_state;
}

QString BatterySimulator::stateText() const
{
    switch (m_state) {
    case ChargeState::Idle:
        return "空闲";
    case ChargeState::WaitCard:
        return "等待刷卡";
    case ChargeState::Charging:
        return "正在充电";
    case ChargeState::Full:
        return "满电";
    case ChargeState::Alarm:
        return "报警停机";
    }
    return "未知";
}

QString BatterySimulator::cardId() const
{
    return m_cardId;
}

void BatterySimulator::start()
{
    m_chargeTimer.restart();
    m_hasCardTimer = false;
    m_parameters.initialPower = qMin(m_parameters.batteryPower, m_parameters.batteryPowerLimit);
    m_state = ChargeState::WaitCard;
}

void BatterySimulator::stop()
{
    m_hasCardTimer = false;
    m_alarms = {};
    m_parameters.voltage = 0;
    m_parameters.current = 0;
    m_parameters.temperature = 32;
    m_state = ChargeState::Idle;
}

void BatterySimulator::acceptCard(const QString &cardId)
{
    m_cardId = cardId;
    if (m_state == ChargeState::WaitCard) {
        m_chargeTimer.restart();
        m_hasCardTimer = true;
        m_parameters.initialPower = qMin(m_parameters.batteryPower, m_parameters.batteryPowerLimit);
        m_state = ChargeState::Charging;
    }
}

void BatterySimulator::tick()
{
    if (m_state != ChargeState::Charging) {
        return;
    }
    if (m_alarms.any()) {
        m_state = ChargeState::Alarm;
        return;
    }

    const double elapsed = m_hasCardTimer ? (m_chargeTimer.elapsed() / 1000.0) : 0.0;
    ChargingParameters &p = m_parameters;
    const int capacity = qMax(1, p.batteryPowerLimit);
    const int initial = qMin(p.initialPower, capacity);
    const double power = initial + (capacity - initial) * (1.0 - qExp(-elapsed / 2.0));
    p.batteryPower = qMin(capacity, qMax(initial, qRound(power)));

    const double soc = static_cast<double>(p.batteryPower) / capacity;
    if (soc < 0.8) {
        p.current = qMax(1, qRound(p.currentLimit * 2.0 / 3.0));
        p.voltage = qMax(1, qRound(p.voltageLimit * (0.45 + 0.45 * soc)));
    } else {
        p.voltage = qMax(1, qRound(p.voltageLimit * 2.0 / 3.0));
        const double taper = qMax(0.08, 1.0 - (soc - 0.8) / 0.2);
        p.current = qMax(1, qRound(p.currentLimit * 2.0 / 3.0 * taper));
    }
    p.temperature = qMin(120, 30 + qRound(18 * soc) + static_cast<int>(elapsed) % 4);

    if (p.voltage > p.voltageLimit || p.current > p.currentLimit || p.temperature > p.temperatureLimit) {
        m_state = ChargeState::Alarm;
        return;
    }

    if (p.batteryPower >= qRound(p.batteryPowerLimit * 0.99)) {
        p.batteryPower = p.batteryPowerLimit;
        m_state = ChargeState::Full;
    }
}

void BatterySimulator::setManualAlarms(bool overVoltage, bool overCurrent, bool overTemperature)
{
    m_alarms.overVoltage = overVoltage;
    m_alarms.overCurrent = overCurrent;
    m_alarms.overTemperature = overTemperature;
    ChargingParameters &p = m_parameters;
    if (overVoltage) {
        p.voltage = qMax(p.voltage, p.voltageLimit + qMax(1, p.voltageLimit / 20));
    } else if (p.voltage > p.voltageLimit) {
        p.voltage = qMax(1, qRound(p.voltageLimit * 2.0 / 3.0));
    }
    if (overCurrent) {
        p.current = qMax(p.current, p.currentLimit + qMax(1, p.currentLimit / 20));
    } else if (p.current > p.currentLimit) {
        p.current = qMax(1, qRound(p.currentLimit * 2.0 / 3.0));
    }
    if (overTemperature) {
        p.temperature = qMax(p.temperature, p.temperatureLimit + 5);
    } else if (p.temperature > p.temperatureLimit) {
        p.temperature = qMax(25, p.temperatureLimit - 5);
    }
    if (m_state == ChargeState::Charging && m_alarms.any()) {
        m_state = ChargeState::Alarm;
    }
}

AlarmFlags BatterySimulator::alarms() const
{
    return m_alarms;
}

QString BatterySimulator::effectiveAlarmText() const
{
    AlarmFlags flags = m_alarms;
    flags.overVoltage = flags.overVoltage || m_parameters.voltage > m_parameters.voltageLimit;
    flags.overCurrent = flags.overCurrent || m_parameters.current > m_parameters.currentLimit;
    flags.overTemperature = flags.overTemperature || m_parameters.temperature > m_parameters.temperatureLimit;
    return flags.text();
}

}
