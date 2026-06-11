#include "core/crc16.h"
#include "core/modbusframe.h"
#include "core/registermap.h"
#include "services/collectorservice.h"
#include "services/controllerservice.h"
#include "transport/virtualbus.h"

#include <QCoreApplication>
#include <QDebug>
#include <QThread>
#include <QtMath>

static int failures = 0;

void check(bool condition, const QString &name)
{
    if (condition) {
        qInfo() << "PASS" << name;
    } else {
        qWarning() << "FAIL" << name;
        ++failures;
    }
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const QByteArray readAll = QByteArray::fromHex("010400000008F1CC");
    check(Core::crc16Modbus(QByteArray::fromHex("010400000008")) == 0xCCF1, "CRC known vector");
    check(Core::verifyCrc(readAll), "CRC verify");

    const QVector<quint16> card = Core::cardIdToBcdRegisters("2428403001");
    check(card == QVector<quint16>({0x2428, 0x4030, 0x0100}), "BCD encode");
    check(Core::bcdRegistersToCardId(card) == "2428403001", "BCD decode");

    Services::CollectorService collector(1);
    Transport::VirtualBus bus;
    bus.setHandler([&collector](const QByteArray &payload) {
        return collector.handleRequest(payload);
    });
    Services::ControllerService controller(bus.endpoint(), 1);
    controller.setPlugged(true);
    check(controller.startCharge(), "start charge");
    const Services::ControllerSnapshot waitingSnapshot = controller.pollParameters();
    QThread::msleep(1200);
    const Services::ControllerSnapshot stillWaitingSnapshot = controller.pollParameters();
    check(controller.snapshot().waitingForCard && !controller.snapshot().charging, "wait for card before charging");
    check(stillWaitingSnapshot.batteryPower == waitingSnapshot.batteryPower, "battery unchanged before card");
    check(controller.writeCard("2428403001"), "write card");
    const Services::ControllerSnapshot snapshot = controller.pollParameters();
    check(snapshot.batteryPowerLimit > 0 && snapshot.voltage > 0, "poll parameters");
    QThread::msleep(1200);
    const Services::ControllerSnapshot laterSnapshot = controller.pollParameters();
    check(laterSnapshot.batteryPower > snapshot.batteryPower, "battery power increases while charging");
    const int exponentialFloor = snapshot.batteryPower
                                 + qRound((snapshot.batteryPowerLimit - snapshot.batteryPower) * 0.30);
    check(laterSnapshot.batteryPower >= exponentialFloor, "exponential battery model");
    Core::ChargingParameters &nearFull = collector.simulator().parameters();
    nearFull.batteryPowerLimit = 200;
    nearFull.batteryPower = 199;
    nearFull.initialPower = 199;
    const QByteArray stopFrame = Core::ModbusFrame::buildWriteCoil(1, Core::ChargeControlCoil, false);
    controller.pollParameters();
    bool autoStopFrameSent = false;
    for (const Transport::BusLogEntry &entry : bus.log()) {
        autoStopFrameSent = autoStopFrameSent || (entry.direction == "TX" && entry.payload == stopFrame);
    }
    check(!controller.snapshot().charging, "auto stop at full");
    check(autoStopFrameSent, "auto stop frame sent");

    const QByteArray bad = collector.handleRequest(Core::ModbusFrame::buildReadRegisters(1, 0x000B, 2));
    Core::ModbusFrame badFrame = Core::ModbusFrame::parse(bad);
    check(badFrame.function() == 0x84 && badFrame.data() == QByteArray::fromHex("02"), "illegal register exception");

    const QByteArray badCoil = collector.handleRequest(Core::ModbusFrame::buildWriteCoil(1, 0x0001, true));
    Core::ModbusFrame badCoilFrame = Core::ModbusFrame::parse(badCoil);
    check(badCoilFrame.function() == 0x85 && badCoilFrame.data() == QByteArray::fromHex("02"), "illegal coil exception");

    const QByteArray badCoilValue = collector.handleRequest(Core::ModbusFrame(1, Core::WriteSingleCoil, QByteArray::fromHex("00000001")).toBytes());
    Core::ModbusFrame badCoilValueFrame = Core::ModbusFrame::parse(badCoilValue);
    check(badCoilValueFrame.function() == 0x85 && badCoilValueFrame.data() == QByteArray::fromHex("03"), "illegal coil value exception");

    const QByteArray badFunction = collector.handleRequest(Core::ModbusFrame(1, 0x03, QByteArray()).toBytes());
    Core::ModbusFrame badFunctionFrame = Core::ModbusFrame::parse(badFunction);
    check(badFunctionFrame.function() == 0x83 && badFunctionFrame.data() == QByteArray::fromHex("01"), "illegal function exception");

    qInfo() << "Failures:" << failures;
    return failures == 0 ? 0 : 1;
}
