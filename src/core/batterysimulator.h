#ifndef BATTERYSIMULATOR_H
#define BATTERYSIMULATOR_H

#include <QElapsedTimer>
#include <QString>

namespace Core {

enum class ChargeState {
    Idle,
    WaitCard,
    Charging,
    Full,
    Alarm
};

struct AlarmFlags {
    bool overVoltage = false;
    bool overCurrent = false;
    bool overTemperature = false;

    bool any() const;
    QString text() const;
};

struct ChargingParameters {
    int voltage = 100;
    int current = 50;
    int voltageLimit = 240;
    int currentLimit = 90;
    int temperature = 32;
    int temperatureLimit = 70;
    int batteryPower = 30;
    int batteryPowerLimit = 200;
    int initialPower = 30;
};

class BatterySimulator
{
public:
    ChargingParameters &parameters();
    const ChargingParameters &parameters() const;
    ChargeState state() const;
    QString stateText() const;
    QString cardId() const;

    void start();
    void stop();
    void acceptCard(const QString &cardId);
    void tick();
    void setManualAlarms(bool overVoltage, bool overCurrent, bool overTemperature);
    AlarmFlags alarms() const;
    QString effectiveAlarmText() const;

private:
    ChargingParameters m_parameters;
    ChargeState m_state = ChargeState::Idle;
    AlarmFlags m_alarms;
    QString m_cardId = "2428403001";
    QElapsedTimer m_chargeTimer;
    bool m_hasCardTimer = false;
};

}

#endif

