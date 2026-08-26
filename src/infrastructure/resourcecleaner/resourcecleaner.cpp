#include "resourcecleaner.h"
#include "acsource.h"
#include "dcload.h"
#include "relay.h"
#include "icommunication.h"
#include <QDebug>

// ========================================
// 清理單個通信對象（內部輔助方法）
// ========================================

void ResourceCleaner::cleanupCommunication(ICommunication* comm)
{
    if (comm) {
        // 1. 如果連接是打開的，先關閉
        if (comm->isOpen()) {
            comm->close();
            qDebug() << "[ResourceCleaner] Communication closed";
        }

        // 2. 刪除對象
        delete comm;
    }
}

// ========================================
// 清理 AC Source
// ========================================

void ResourceCleaner::cleanupACSource(
    ACSource* source,
    ICommunication* comm)
{
    qDebug() << "[ResourceCleaner] Cleaning up AC Source...";

    // 1. 刪除 AC Source 對象
    if (source) {
        delete source;
        qDebug() << "[ResourceCleaner] AC Source deleted";
    }

    // 2. 清理通信對象
    cleanupCommunication(comm);

    qDebug() << "[ResourceCleaner] AC Source cleanup completed";
}

// ========================================
// 清理 DC Loads
// ========================================

void ResourceCleaner::cleanupDCLoads(
    QVector<DCLoad*>& dcLoads,
    QMap<QString, ICommunication*>& commMap)
{
    qDebug() << "[ResourceCleaner] Cleaning up DC Loads...";
    qDebug() << "[ResourceCleaner] DC Load count:" << dcLoads.size()
             << "Comm count:" << commMap.size();

    // 1. 刪除所有 DC Load 對象
    for (auto* dcLoad : dcLoads) {
        if (dcLoad) {
            delete dcLoad;
        }
    }
    dcLoads.clear();
    qDebug() << "[ResourceCleaner] All DC Loads deleted";

    // 2. 關閉並刪除所有通信對象
    for (auto* comm : commMap) {
        cleanupCommunication(comm);
    }
    commMap.clear();
    qDebug() << "[ResourceCleaner] All communications cleaned";

    qDebug() << "[ResourceCleaner] DC Loads cleanup completed";
}

// ========================================
// 清理 Relays
// ========================================

void ResourceCleaner::cleanupRelays(
    QVector<RelayBase*>& relays,
    QMap<QString, ICommunication*>& commMap)
{
    qDebug() << "[ResourceCleaner] Cleaning up Relays...";
    qDebug() << "[ResourceCleaner] Relay count:" << relays.size()
             << "Comm count:" << commMap.size();

    // 1. 刪除所有 Relay 對象
    for (auto* relay : relays) {
        if (relay) {
            delete relay;
        }
    }
    relays.clear();
    qDebug() << "[ResourceCleaner] All Relays deleted";

    // 2. 關閉並刪除所有通信對象
    for (auto* comm : commMap) {
        cleanupCommunication(comm);
    }
    commMap.clear();
    qDebug() << "[ResourceCleaner] All communications cleaned";

    qDebug() << "[ResourceCleaner] Relays cleanup completed";
}
