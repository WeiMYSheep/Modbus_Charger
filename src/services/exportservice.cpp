#include "exportservice.h"

#include <QFile>
#include <QTextStream>

namespace Services {

bool writeTextFile(const QString &path, const QString &text)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << text;
    return true;
}

bool ExportService::exportBusLog(const QVector<Transport::BusLogEntry> &entries, const QString &path) const
{
    return writeTextFile(path, logText(entries));
}

bool ExportService::exportHistory(const QVector<ControllerSnapshot> &snapshots, const QString &path) const
{
    return writeTextFile(path, historyCsv(snapshots));
}

bool ExportService::exportBill(const QString &cardId, const QVector<ControllerSnapshot> &snapshots, const QString &path) const
{
    double energy = 0.0;
    if (snapshots.size() >= 2) {
        energy = qMax(0, snapshots.last().batteryPower - snapshots.first().batteryPower);
    }
    const double fee = energy * 1.2;
    QString text;
    QTextStream out(&text);
    out << "cardId,energyKwh,feeYuan,sampleCount\n";
    out << cardId << ',' << QString::number(energy, 'f', 2) << ','
        << QString::number(fee, 'f', 2) << ',' << snapshots.size() << '\n';
    return writeTextFile(path, text);
}

QString ExportService::historyCsv(const QVector<ControllerSnapshot> &snapshots) const
{
    QString text;
    QTextStream out(&text);
    out << "voltage,current,temperature,batteryPower,batteryLimit,socPercent,progress,"
           "voltageMargin,currentMargin,temperatureMargin,riskLevel,alarm\n";
    for (const ControllerSnapshot &snapshot : snapshots) {
        const int voltageMargin = snapshot.voltageLimit - snapshot.voltage;
        const int currentMargin = snapshot.currentLimit - snapshot.current;
        const int temperatureMargin = snapshot.temperatureLimit - snapshot.temperature;
        QString riskLevel = "normal";
        if (snapshot.alarmText != QStringLiteral("正常") || voltageMargin < 0 || currentMargin < 0 || temperatureMargin < 0) {
            riskLevel = "alarm";
        } else if (voltageMargin < snapshot.voltageLimit * 0.10
                   || currentMargin < snapshot.currentLimit * 0.10
                   || temperatureMargin < snapshot.temperatureLimit * 0.10) {
            riskLevel = "near_limit";
        } else if (snapshot.temperature >= 55 || snapshot.progress >= 90.0) {
            riskLevel = "watch";
        }
        const double soc = snapshot.batteryPower * 100.0 / qMax(1, snapshot.batteryPowerLimit);
        out << snapshot.voltage << ',' << snapshot.current << ',' << snapshot.temperature << ','
            << snapshot.batteryPower << ',' << snapshot.batteryPowerLimit << ','
            << QString::number(soc, 'f', 2) << ','
            << QString::number(snapshot.progress, 'f', 2) << ','
            << voltageMargin << ',' << currentMargin << ',' << temperatureMargin << ','
            << riskLevel << ',' << snapshot.alarmText << '\n';
    }
    return text;
}

QString ExportService::logText(const QVector<Transport::BusLogEntry> &entries) const
{
    QString text;
    QTextStream out(&text);
    for (const Transport::BusLogEntry &entry : entries) {
        out << entry.format() << '\n';
    }
    return text;
}

}
