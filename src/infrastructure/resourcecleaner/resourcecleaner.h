#pragma once

#include <QVector>
#include <QMap>
#include <QString>

// 前向聲明
class ACSource;
class DCLoad;
class RelayBase;
class ICommunication;

/**
 * @brief 資源清理器
 *
 * 提供統一的資源清理方法，確保沒有內存洩漏
 * 自動處理通信對象的關閉和刪除
 *
 * 使用范例：
 * @code
 * // 清理 AC Source
 * ResourceCleaner::cleanupACSource(source, comm);
 *
 * // 清理 DC Loads
 * ResourceCleaner::cleanupDCLoads(dcLoads, commMap);
 * @endcode
 */
class ResourceCleaner {
public:
    /**
     * @brief 清理 AC Source 資源
     * @param source AC Source 對象指針
     * @param comm 通信對象指針
     */
    static void cleanupACSource(
        ACSource* source,
        ICommunication* comm
        );

    /**
     * @brief 清理 DC Load 資源
     * @param dcLoads DC Load 對象列表（會被清空）
     * @param commMap 通信對象映射（會被清空）
     */
    static void cleanupDCLoads(
        QVector<DCLoad*>& dcLoads,
        QMap<QString, ICommunication*>& commMap
        );

    /**
     * @brief 清理 Relay 資源
     * @param relays Relay 對象列表（會被清空）
     * @param commMap 通信對象映射（會被清空）
     */
    static void cleanupRelays(
        QVector<RelayBase*>& relays,
        QMap<QString, ICommunication*>& commMap
        );

private:
    /**
     * @brief 清理單個通信對象（內部輔助方法）
     *
     * 先關閉連接，再刪除對象
     *
     * @param comm 通信對象指針
     */
    static void cleanupCommunication(ICommunication* comm);

    // 禁止實例化
    ResourceCleaner() = delete;
    ~ResourceCleaner() = delete;
    ResourceCleaner(const ResourceCleaner&) = delete;
    ResourceCleaner& operator=(const ResourceCleaner&) = delete;
};
