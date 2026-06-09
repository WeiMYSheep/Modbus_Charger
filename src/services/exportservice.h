#ifndef EXPORTSERVICE_H
#define EXPORTSERVICE_H

#include "services/controllerservice.h"
#include "transport/virtualbus.h"

#include <QString>
#include <QVector>

namespace Services {

class ExportService
{
public:
    bool exportBusLog(const QVector<Transport::BusLogEntry> &entries, const QString &path) const;
    bool exportHistory(const QVector<ControllerSnapshot> &snapshots, const QString &path) const;
    bool exportBill(const QString &cardId, const QVector<ControllerSnapshot> &snapshots, const QString &path) const;
    QString historyCsv(const QVector<ControllerSnapshot> &snapshots) const;
    QString logText(const QVector<Transport::BusLogEntry> &entries) const;
};

}

#endif

