#pragma once

#include <QString>
#include <QVector>
#include "page2config.h"

/**
 * @brief 數據查找器
 *
 * 提供從 Page2 數據中查找選定配置的靜態方法
 *
 * 使用范例：
 * @code
 * auto result = DataFinder::findLoadData(loadRows, selectedIndex);
 * if (result.found) {
 *     // 使用 result.values
 * }
 * @endcode
 */
class DataFinder {
public:
    /**
     * @brief Load 數據結果
     */
    struct LoadDataResult {
        QVector<QString> values;  ///< 值列表
        bool found = false;       ///< 是否找到
    };

    /**
     * @brief Dynamic Load 數據結果
     */
    struct DyLoadDataResult {
        QVector<QString> values;  ///< 值列表
        QString t1t2;             ///< T1/T2 時間 (ms)
        bool found = false;       ///< 是否找到
    };

    /**
     * @brief Relay 數據結果
     */
    struct RelayDataResult {
        QVector<QString> targetStates;  ///< 目標狀態 ["ON", "OFF", ...]
        bool found = false;              ///< 是否找到
    };

    /**
     * @brief 查找 Load 數據
     * @param rows Load 數據行
     * @param selectedIndex 選擇的索引
     * @return 查找結果
     */
    static LoadDataResult findLoadData(
        const QVector<LoadDataRow>& rows,
        int selectedIndex
        );

    /**
     * @brief 查找 Dynamic Load 數據
     * @param rows Dynamic Load 數據行
     * @param selectedIndex 選擇的索引
     * @param t1t2Vector T1/T2 時間向量 (ms)
     * @return 查找結果
     */
    static DyLoadDataResult findDyLoadData(
        const QVector<DynamicDataRow>& rows,
        int selectedIndex,
        const QVector<QString>& t1t2Vector
        );

    /**
     * @brief 查找 Relay 數據
     * @param rows Relay 數據行
     * @param conditionIndex 條件索引
     * @return 查找結果
     */
    static RelayDataResult findRelayData(
        const QVector<RelayDataRow>& rows,
        int conditionIndex
        );

private:
    // 禁止實例化
    DataFinder() = delete;
    ~DataFinder() = delete;
    DataFinder(const DataFinder&) = delete;
    DataFinder& operator=(const DataFinder&) = delete;
};
