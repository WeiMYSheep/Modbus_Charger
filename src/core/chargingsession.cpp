#include "chargingsession.h"

#include <numeric>

namespace Core {

double TelemetrySample::progress() const
{
    return qMin(100.0, batteryPower * 100.0 / qMax(1, batteryLimit));
}

double TelemetrySample::instantPowerKw() const
{
    return voltage * current / 1000.0;
}

double FeePlan::estimate(double energyKwh, double durationHours) const
{
    return energyKwh * (energyPrice + servicePrice) + durationHours * parkingPricePerHour;
}

ChargingSession::ChargingSession(QString cardId)
    : m_cardId(std::move(cardId)), m_startedAt(QDateTime::currentDateTime())
{
}

void ChargingSession::addSample(const TelemetrySample &sample)
{
    m_samples.append(sample);
}

void ChargingSession::stop(const QString &reason)
{
    m_stoppedAt = QDateTime::currentDateTime();
    m_stopReason = reason;
}

QString ChargingSession::cardId() const
{
    return m_cardId;
}

QVector<TelemetrySample> ChargingSession::samples() const
{
    return m_samples;
}

double ChargingSession::durationSeconds() const
{
    const QDateTime end = m_stoppedAt.isValid() ? m_stoppedAt : QDateTime::currentDateTime();
    return m_startedAt.msecsTo(end) / 1000.0;
}

double ChargingSession::energyKwh() const
{
    if (m_samples.size() < 2) {
        return 0.0;
    }
    return qMax(0, m_samples.last().batteryPower - m_samples.first().batteryPower);
}

double ChargingSession::fee() const
{
    return m_feePlan.estimate(energyKwh(), durationSeconds() / 3600.0);
}

double ChargingSession::averageVoltage() const
{
    if (m_samples.isEmpty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (const TelemetrySample &sample : m_samples) {
        sum += sample.voltage;
    }
    return sum / m_samples.size();
}

double ChargingSession::averageCurrent() const
{
    if (m_samples.isEmpty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (const TelemetrySample &sample : m_samples) {
        sum += sample.current;
    }
    return sum / m_samples.size();
}

int ChargingSession::maxTemperature() const
{
    int maxValue = 0;
    for (const TelemetrySample &sample : m_samples) {
        maxValue = qMax(maxValue, sample.temperature);
    }
    return maxValue;
}

QMap<QString, QString> ChargingSession::summary() const
{
    return {
        {"cardId", m_cardId},
        {"startedAt", m_startedAt.toString("yyyy-MM-dd HH:mm:ss")},
        {"stoppedAt", m_stoppedAt.isValid() ? m_stoppedAt.toString("yyyy-MM-dd HH:mm:ss") : ""},
        {"durationSeconds", QString::number(durationSeconds(), 'f', 1)},
        {"energyKwh", QString::number(energyKwh(), 'f', 2)},
        {"fee", QString::number(fee(), 'f', 2)},
        {"averageVoltage", QString::number(averageVoltage(), 'f', 2)},
        {"averageCurrent", QString::number(averageCurrent(), 'f', 2)},
        {"maxTemperature", QString::number(maxTemperature())},
        {"stopReason", m_stopReason}
    };
}

void SessionLedger::begin(const QString &cardId)
{
    if (m_active.has_value()) {
        m_active->stop("新会话开始，自动结束上一会话");
        m_sessions.append(*m_active);
    }
    m_active = ChargingSession(cardId);
}

void SessionLedger::sample(const TelemetrySample &sample)
{
    if (m_active.has_value()) {
        m_active->addSample(sample);
    }
}

QMap<QString, QString> SessionLedger::end(const QString &reason)
{
    if (!m_active.has_value()) {
        return {};
    }
    m_active->stop(reason);
    const QMap<QString, QString> result = m_active->summary();
    m_sessions.append(*m_active);
    m_active.reset();
    return result;
}

QVector<QMap<QString, QString>> SessionLedger::summaries() const
{
    QVector<QMap<QString, QString>> rows;
    for (const ChargingSession &session : m_sessions) {
        rows.append(session.summary());
    }
    if (m_active.has_value()) {
        rows.append(m_active->summary());
    }
    return rows;
}

double SessionLedger::totalEnergy() const
{
    double sum = 0.0;
    for (const auto &row : summaries()) {
        sum += row.value("energyKwh").toDouble();
    }
    return sum;
}

double SessionLedger::totalFee() const
{
    double sum = 0.0;
    for (const auto &row : summaries()) {
        sum += row.value("fee").toDouble();
    }
    return sum;
}

}

