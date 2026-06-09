#ifndef ALARMPOLICY_H
#define ALARMPOLICY_H

#include <QString>
#include <QVector>

namespace Core {

enum class AlarmSeverity {
    Normal,
    Notice,
    Warning,
    Critical
};

struct AlarmEvent {
    QString code;
    QString name;
    AlarmSeverity severity = AlarmSeverity::Normal;
    int measured = 0;
    int limit = 0;
    QString message;
};

class AlarmPolicy
{
public:
    explicit AlarmPolicy(double warningRatio = 0.9);

    AlarmEvent evaluateVoltage(int voltage, int limit) const;
    AlarmEvent evaluateCurrent(int current, int limit) const;
    AlarmEvent evaluateTemperature(int temperature, int limit) const;
    QVector<AlarmEvent> evaluateAll(int voltage,
                                    int voltageLimit,
                                    int current,
                                    int currentLimit,
                                    int temperature,
                                    int temperatureLimit) const;
    QString summary(const QVector<AlarmEvent> &events) const;
    bool shouldStop(const QVector<AlarmEvent> &events) const;
    QStringList formatEvents(const QVector<AlarmEvent> &events) const;

private:
    AlarmEvent evaluateOne(const QString &code, const QString &name, int measured, int limit, const QString &label) const;
    double m_warningRatio;
};

QString severityText(AlarmSeverity severity);

}

#endif

