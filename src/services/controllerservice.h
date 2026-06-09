#ifndef CONTROLLERSERVICE_H
#define CONTROLLERSERVICE_H

#include "core/modbusframe.h"
#include "transport/virtualbus.h"

#include <QObject>
#include <QString>

namespace Services {

struct ControllerSnapshot {
    int voltage = 0;
    int current = 0;
    int voltageLimit = 0;
    int currentLimit = 0;
    int temperature = 0;
    int temperatureLimit = 0;
    int batteryPower = 0;
    int batteryPowerLimit = 1;
    QString cardId;
    bool charging = false;
    bool waitingForCard = false;
    bool plugged = false;
    QString alarmText = "正常";
    double progress = 0.0;
    QString lastError;
};

class ControllerService : public QObject
{
    Q_OBJECT
public:
    explicit ControllerService(Transport::Endpoint endpoint, quint8 address = 1, QObject *parent = nullptr);

    ControllerSnapshot snapshot() const;
    void setPlugged(bool plugged);
    void setAddress(quint8 address);
    void setEndpoint(Transport::Endpoint endpoint);
    quint8 address() const;

public slots:
    bool startCharge();
    bool stopCharge();
    bool writeCard(const QString &cardId);
    ControllerSnapshot pollParameters();
    QString readCard();

signals:
    void snapshotChanged(const Services::ControllerSnapshot &snapshot);

private:
    bool expectEcho(const QByteArray &response, quint8 function);
    bool parseResponse(const QByteArray &response, Core::ModbusFrame *frame);
    void updateAlarmText();

    Transport::Endpoint m_endpoint;
    quint8 m_address;
    ControllerSnapshot m_snapshot;
};

}

#endif
