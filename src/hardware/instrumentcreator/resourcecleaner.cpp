#include "resourcecleaner.h"
#include "acsource.h"
#include "dcload.h"
#include "relay.h"
#include "icommunication.h"
#include <QDebug>
#include <memory>
#include <exception>

void ResourceCleaner::cleanupCommunication(ICommunication *comm)
{
    std::unique_ptr<ICommunication> owner(comm);
    if (!owner)
        return;
    try {
        if (owner->isOpen())
            owner->close();
    } catch (const std::exception &error) {
        qWarning() << "Communication close failed during resource release:" << error.what();
    } catch (...) {
        qWarning() << "Communication close failed during resource release";
    }
}
void ResourceCleaner::cleanupACSource(ACSource *source, ICommunication *comm)
{
    delete source;
    cleanupCommunication(comm);
}
void ResourceCleaner::cleanupDCLoads(QVector<DCLoad *> &loads,
                                     QMap<QString, ICommunication *> &communications)
{
    qDeleteAll(loads);
    loads.clear();
    for (auto *communication : communications)
        cleanupCommunication(communication);
    communications.clear();
}
void ResourceCleaner::cleanupRelays(QVector<RelayBase *> &relays,
                                    QMap<QString, ICommunication *> &communications)
{
    qDeleteAll(relays);
    relays.clear();
    for (auto *communication : communications)
        cleanupCommunication(communication);
    communications.clear();
}
