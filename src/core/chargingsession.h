#ifndef CHARGINGSESSION_H
#define CHARGINGSESSION_H

#include <QDateTime>
#include <QMap>
#include <QString>
#include <QVector>
#include <optional>

namespace Core {

struct TelemetrySample {
    QDateTime timestamp;
    int voltage = 0;
    int current = 0;
    int temperature = 0;
    int batteryPower = 0;
    int batteryLimit = 1;
    QString alarm = "正常";

    double progress() const;
    double instantPowerKw() const;
};

struct FeePlan {
    double energyPrice = 0.85;
    double servicePrice = 0.35;
    double parkingPricePerHour = 0.0;

    double estimate(double energyKwh, double durationHours) const;
};

class ChargingSession
{
public:
    explicit ChargingSession(QString cardId = {});

    void addSample(const TelemetrySample &sample);
    void stop(const QString &reason);
    QString cardId() const;
    QVector<TelemetrySample> samples() const;
    double durationSeconds() const;
    double energyKwh() const;
    double fee() const;
    double averageVoltage() const;
    double averageCurrent() const;
    int maxTemperature() const;
    QMap<QString, QString> summary() const;

private:
    QString m_cardId;
    QDateTime m_startedAt;
    QDateTime m_stoppedAt;
    QString m_stopReason;
    QVector<TelemetrySample> m_samples;
    FeePlan m_feePlan;
};

class SessionLedger
{
public:
    void begin(const QString &cardId);
    void sample(const TelemetrySample &sample);
    QMap<QString, QString> end(const QString &reason);
    QVector<QMap<QString, QString>> summaries() const;
    double totalEnergy() const;
    double totalFee() const;

private:
    QVector<ChargingSession> m_sessions;
    std::optional<ChargingSession> m_active;
};

}

#endif
