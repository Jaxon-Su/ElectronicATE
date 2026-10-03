#pragma once
#include <QVector>
#include <QMap>
#include <QString>
class ACSource;
class DCLoad;
class RelayBase;
class ICommunication;

// Release drivers before their borrowed transports. Close failures must not abort remaining releases.
// Output OFF/readback belongs to the executor before this memory/connection cleanup.
class ResourceCleaner
{
  public:
    static void cleanupACSource(ACSource *source, ICommunication *comm);
    static void cleanupDCLoads(QVector<DCLoad *> &loads, QMap<QString, ICommunication *> &communications);
    static void cleanupRelays(QVector<RelayBase *> &relays, QMap<QString, ICommunication *> &communications);

  private:
    static void cleanupCommunication(ICommunication *comm);
    ResourceCleaner() = delete;
};
