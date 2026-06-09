#ifndef MODBUSLABSERVICE_H
#define MODBUSLABSERVICE_H

#include <QByteArray>
#include <QString>
#include <QVector>

namespace Services {

struct LabFrame {
    QString title;
    QString description;
    QByteArray raw;
    QString hex() const;
    QString explanation() const;
};

class ModbusLabService
{
public:
    LabFrame buildReadAllParameters(quint8 address = 1) const;
    LabFrame buildReadCard(quint8 address = 1) const;
    LabFrame buildStart(quint8 address = 1) const;
    LabFrame buildStop(quint8 address = 1) const;
    LabFrame buildWriteCard(const QVector<quint16> &registers, quint8 address = 1) const;
    LabFrame buildInvalidRegisterDemo(quint8 address = 1) const;
    QVector<LabFrame> examples(quint8 address = 1) const;
    QString examplesText(quint8 address = 1) const;
    QString compareFrames(const QByteArray &expected, const QByteArray &actual) const;
};

}

#endif

