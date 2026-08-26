#include "datafinder.h"
#include <QDebug>

// ========================================
// Find Load Data
// ========================================

DataFinder::LoadDataResult DataFinder::findLoadData(
    const QVector<LoadDataRow>& rows,
    int selectedIndex)
{
    LoadDataResult result;
    result.found = false;

    // 檢查索引有效性
    if (selectedIndex < 0 || selectedIndex >= rows.size()) {
        qWarning() << "[DataFinder::Load] Invalid index:" << selectedIndex
                   << "Size:" << rows.size();
        return result;
    }

    // 獲取數據
    const LoadDataRow& row = rows[selectedIndex];

    if (row.values.isEmpty()) {
        qWarning() << "[DataFinder::Load] Empty values at index:" << selectedIndex;
        return result;
    }

    qDebug() << "[DataFinder::Load] Found data - Index:" << selectedIndex
             << "Values count:" << row.values.size();

    result.values = row.values;
    result.found = true;
    return result;
}

// ========================================
// Find DyLoad Data
// ========================================

DataFinder::DyLoadDataResult DataFinder::findDyLoadData(
    const QVector<DynamicDataRow>& rows,
    int selectedIndex,
    const QVector<QString>& t1t2Vector)
{
    DyLoadDataResult result;
    result.found = false;

    // 檢查索引有效性
    if (selectedIndex < 0 || selectedIndex >= rows.size()) {
        qWarning() << "[DataFinder::DyLoad] Invalid index:" << selectedIndex
                   << "Size:" << rows.size();
        return result;
    }

    // 獲取數據
    const DynamicDataRow& row = rows[selectedIndex];

    if (row.values.isEmpty()) {
        qWarning() << "[DataFinder::DyLoad] Empty values at index:" << selectedIndex;
        return result;
    }

    // 獲取 T1/T2
    if (selectedIndex >= 0 && selectedIndex < t1t2Vector.size()) {
        result.t1t2 = t1t2Vector[selectedIndex];
    } else {
        qWarning() << "[DataFinder::DyLoad] T1/T2 not found for index:" << selectedIndex;
        result.t1t2 = "";  // 空值：applyDyLoadValueSettings 會套用 0.01/0.01ms 預設
    }

    qDebug() << "[DataFinder::DyLoad] Found data - Index:" << selectedIndex
             << "Values count:" << row.values.size()
             << "T1/T2:" << result.t1t2;

    result.values = row.values;
    result.found = true;
    return result;
}

// ========================================
// Find Relay Data
// ========================================

DataFinder::RelayDataResult DataFinder::findRelayData(
    const QVector<RelayDataRow>& rows,
    int conditionIndex)
{
    RelayDataResult result;
    result.found = false;

    // 檢查索引有效性
    if (conditionIndex < 0 || conditionIndex >= rows.size()) {
        qWarning() << "[DataFinder::Relay] Invalid index:" << conditionIndex
                   << "Size:" << rows.size();
        return result;
    }

    // 獲取目標狀態列表
    const QVector<QString>& targetStates = rows[conditionIndex].values;

    if (targetStates.isEmpty()) {
        qWarning() << "[DataFinder::Relay] Empty states at index:" << conditionIndex;
        return result;
    }

    qDebug() << "[DataFinder::Relay] Found data - Condition:" << conditionIndex
             << "States count:" << targetStates.size();

    result.targetStates = targetStates;
    result.found = true;
    return result;
}
