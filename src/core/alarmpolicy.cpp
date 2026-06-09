#include "alarmpolicy.h"

#include <QStringList>

namespace Core {

AlarmPolicy::AlarmPolicy(double warningRatio)
    : m_warningRatio(warningRatio)
{
}

AlarmEvent AlarmPolicy::evaluateVoltage(int voltage, int limit) const
{
    return evaluateOne("OV", "过压", voltage, limit, "电压");
}

AlarmEvent AlarmPolicy::evaluateCurrent(int current, int limit) const
{
    return evaluateOne("OC", "过流", current, limit, "电流");
}

AlarmEvent AlarmPolicy::evaluateTemperature(int temperature, int limit) const
{
    return evaluateOne("OT", "高温", temperature, limit, "温度");
}

QVector<AlarmEvent> AlarmPolicy::evaluateAll(int voltage,
                                             int voltageLimit,
                                             int current,
                                             int currentLimit,
                                             int temperature,
                                             int temperatureLimit) const
{
    QVector<AlarmEvent> events;
    const QVector<AlarmEvent> candidates = {
        evaluateVoltage(voltage, voltageLimit),
        evaluateCurrent(current, currentLimit),
        evaluateTemperature(temperature, temperatureLimit)
    };
    for (const AlarmEvent &event : candidates) {
        if (event.severity != AlarmSeverity::Normal) {
            events.append(event);
        }
    }
    return events;
}

QString AlarmPolicy::summary(const QVector<AlarmEvent> &events) const
{
    if (events.isEmpty()) {
        return "正常";
    }
    QStringList critical;
    QStringList warning;
    for (const AlarmEvent &event : events) {
        if (event.severity == AlarmSeverity::Critical) {
            critical << event.name;
        } else if (event.severity == AlarmSeverity::Warning) {
            warning << event.name;
        }
    }
    critical.removeDuplicates();
    warning.removeDuplicates();
    if (!critical.isEmpty()) {
        return critical.join("、");
    }
    if (!warning.isEmpty()) {
        return "预警:" + warning.join("、");
    }
    return "正常";
}

bool AlarmPolicy::shouldStop(const QVector<AlarmEvent> &events) const
{
    for (const AlarmEvent &event : events) {
        if (event.severity == AlarmSeverity::Critical) {
            return true;
        }
    }
    return false;
}

QStringList AlarmPolicy::formatEvents(const QVector<AlarmEvent> &events) const
{
    if (events.isEmpty()) {
        return {"正常"};
    }
    QStringList lines;
    for (const AlarmEvent &event : events) {
        lines << QString("%1[%2]: %3").arg(event.name, severityText(event.severity), event.message);
    }
    return lines;
}

AlarmEvent AlarmPolicy::evaluateOne(const QString &code, const QString &name, int measured, int limit, const QString &label) const
{
    AlarmEvent event;
    event.code = code;
    event.name = name;
    event.measured = measured;
    event.limit = limit;
    if (limit <= 0) {
        event.severity = AlarmSeverity::Critical;
        event.message = label + "门限配置无效";
    } else if (measured > limit) {
        event.severity = AlarmSeverity::Critical;
        event.message = QString("%1%2超过门限%3").arg(label).arg(measured).arg(limit);
    } else if (measured >= qRound(limit * m_warningRatio)) {
        event.severity = AlarmSeverity::Warning;
        event.message = QString("%1%2接近门限%3").arg(label).arg(measured).arg(limit);
    } else {
        event.severity = AlarmSeverity::Normal;
        event.message = "正常";
    }
    return event;
}

QString severityText(AlarmSeverity severity)
{
    switch (severity) {
    case AlarmSeverity::Normal:
        return "正常";
    case AlarmSeverity::Notice:
        return "提示";
    case AlarmSeverity::Warning:
        return "预警";
    case AlarmSeverity::Critical:
        return "严重";
    }
    return "未知";
}

}

