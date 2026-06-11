#include "controllerservice.h"

#include "core/modbusframe.h"
#include "core/registermap.h"

namespace Services {

ControllerService::ControllerService(Transport::Endpoint endpoint, quint8 address, QObject *parent)
    : QObject(parent), m_endpoint(endpoint), m_address(address)
{
}

ControllerSnapshot ControllerService::snapshot() const
{
    return m_snapshot;
}

void ControllerService::setPlugged(bool plugged)
{
    m_snapshot.plugged = plugged;
    emit snapshotChanged(m_snapshot);
}

void ControllerService::setAddress(quint8 address)
{
    m_address = address;
}

void ControllerService::setEndpoint(Transport::Endpoint endpoint)
{
    m_endpoint = std::move(endpoint);
}

quint8 ControllerService::address() const
{
    return m_address;
}

bool ControllerService::startCharge()
{
    if (!m_snapshot.plugged) {
        m_snapshot.lastError = "未插枪，不能启动";
        emit snapshotChanged(m_snapshot);
        return false;
    }
    const QByteArray response = m_endpoint.exchange(Core::ModbusFrame::buildWriteCoil(m_address, Core::ChargeControlCoil, true));
    if (expectEcho(response, Core::WriteSingleCoil)) {
        m_snapshot.charging = false;
        m_snapshot.waitingForCard = true;
        m_snapshot.lastError.clear();
        emit snapshotChanged(m_snapshot);
        return true;
    }
    emit snapshotChanged(m_snapshot);
    return false;
}

bool ControllerService::stopCharge()
{
    if (sendStopFrame()) {
        m_snapshot.charging = false;
        m_snapshot.waitingForCard = false;
        m_snapshot.lastError.clear();
        emit snapshotChanged(m_snapshot);
        return true;
    }
    emit snapshotChanged(m_snapshot);
    return false;
}

bool ControllerService::writeCard(const QString &cardId)
{
    if (!m_snapshot.waitingForCard) {
        m_snapshot.lastError = m_snapshot.charging ? "已在充电，不能重复刷卡" : "请先启动充电流程，再刷卡";
        emit snapshotChanged(m_snapshot);
        return false;
    }
    const QVector<quint16> values = Core::cardIdToBcdRegisters(cardId);
    const QByteArray response = m_endpoint.exchange(Core::ModbusFrame::buildWriteRegisters(m_address, Core::CardId14, values));
    Core::ModbusFrame frame;
    if (parseResponse(response, &frame) && frame.function() == Core::WriteMultipleRegisters && frame.data() == QByteArray::fromHex("00080003")) {
        m_snapshot.cardId = Core::normalizeCardId(cardId);
        m_snapshot.waitingForCard = false;
        m_snapshot.charging = true;
        m_snapshot.lastError.clear();
        emit snapshotChanged(m_snapshot);
        return true;
    }
    emit snapshotChanged(m_snapshot);
    return false;
}

ControllerSnapshot ControllerService::pollParameters()
{
    const QByteArray response = m_endpoint.exchange(Core::ModbusFrame::buildReadRegisters(m_address, Core::Voltage, 8));
    Core::ModbusFrame frame;
    if (!parseResponse(response, &frame)) {
        emit snapshotChanged(m_snapshot);
        return m_snapshot;
    }
    const QByteArray data = frame.data();
    if (frame.function() != Core::ReadInputRegisters || data.size() != 17 || static_cast<unsigned char>(data.at(0)) != 16) {
        m_snapshot.lastError = "查询响应格式错误";
        emit snapshotChanged(m_snapshot);
        return m_snapshot;
    }
    QVector<int *> fields = {
        &m_snapshot.voltage,
        &m_snapshot.current,
        &m_snapshot.voltageLimit,
        &m_snapshot.currentLimit,
        &m_snapshot.temperature,
        &m_snapshot.temperatureLimit,
        &m_snapshot.batteryPower,
        &m_snapshot.batteryPowerLimit
    };
    for (int i = 0; i < fields.size(); ++i) {
        *fields[i] = Core::readU16(data, 1 + i * 2);
    }
    m_snapshot.progress = qMin(100.0, m_snapshot.batteryPower * 100.0 / qMax(1, m_snapshot.batteryPowerLimit));
    updateAlarmText();
    QString terminalStopError;
    if (m_snapshot.charging && (m_snapshot.alarmText != "正常" || m_snapshot.progress >= 99.0)) {
        if (!sendStopFrame()) {
            terminalStopError = m_snapshot.lastError.isEmpty() ? QStringLiteral("自动停机帧发送失败") : m_snapshot.lastError;
        }
        m_snapshot.charging = false;
        m_snapshot.waitingForCard = false;
    }
    m_snapshot.lastError = terminalStopError;
    emit snapshotChanged(m_snapshot);
    return m_snapshot;
}

QString ControllerService::readCard()
{
    const QByteArray response = m_endpoint.exchange(Core::ModbusFrame::buildReadRegisters(m_address, Core::CardId14, 3));
    Core::ModbusFrame frame;
    if (parseResponse(response, &frame) && frame.function() == Core::ReadInputRegisters && frame.data().size() == 7) {
        QVector<quint16> values;
        for (int offset = 1; offset < 7; offset += 2) {
            values.append(Core::readU16(frame.data(), offset));
        }
        m_snapshot.cardId = Core::bcdRegistersToCardId(values);
    }
    emit snapshotChanged(m_snapshot);
    return m_snapshot.cardId;
}

bool ControllerService::expectEcho(const QByteArray &response, quint8 function)
{
    Core::ModbusFrame frame;
    return parseResponse(response, &frame) && frame.function() == function;
}

bool ControllerService::sendStopFrame()
{
    const QByteArray response = m_endpoint.exchange(Core::ModbusFrame::buildWriteCoil(m_address, Core::ChargeControlCoil, false));
    return expectEcho(response, Core::WriteSingleCoil);
}

bool ControllerService::parseResponse(const QByteArray &response, Core::ModbusFrame *frame)
{
    try {
        *frame = Core::ModbusFrame::parse(response);
    } catch (const Core::ProtocolException &exception) {
        m_snapshot.lastError = exception.message();
        return false;
    }
    if (frame->isException()) {
        const int code = frame->data().isEmpty() ? 0 : static_cast<unsigned char>(frame->data().at(0));
        m_snapshot.lastError = QString("异常响应: 功能码 0x%1, 异常码 %2")
                                   .arg(frame->function(), 2, 16, QLatin1Char('0')).toUpper()
                                   .arg(code);
        return false;
    }
    return true;
}

void ControllerService::updateAlarmText()
{
    QStringList alarms;
    if (m_snapshot.voltage > m_snapshot.voltageLimit) {
        alarms << "过压";
    }
    if (m_snapshot.current > m_snapshot.currentLimit) {
        alarms << "过流";
    }
    if (m_snapshot.temperature > m_snapshot.temperatureLimit) {
        alarms << "高温";
    }
    m_snapshot.alarmText = alarms.isEmpty() ? "正常" : alarms.join("、");
}

}
