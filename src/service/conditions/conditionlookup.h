#pragma once

#include <QString>
#include <QVector>
#include "page2config.h"

// Looks up zero-based Page2 condition rows without changing them.
class ConditionLookup {
  public:
    struct LoadDataResult {
        QVector<QString> values;
        bool found = false;
    };

    struct DynamicLoadDataResult {
        QVector<QString> values;
        QString t1t2; ///< T1/T2 時間 (ms)
        bool found = false;
    };

    struct RelayDataResult {
        QVector<QString> targetStates; // ON/OFF for each mapped relay output.
        bool found = false;
    };

    static LoadDataResult findLoadData(const QVector<LoadDataRow>& rows, int selectedIndex);

    static DynamicLoadDataResult findDynamicLoadData(const QVector<DynamicDataRow>& rows, int selectedIndex,
                                           const QVector<QString>& t1t2Vector);

    static RelayDataResult findRelayData(const QVector<RelayDataRow>& rows, int conditionIndex);

  private:
    ConditionLookup() = delete;
    ~ConditionLookup() = delete;
    ConditionLookup(const ConditionLookup&) = delete;
    ConditionLookup& operator=(const ConditionLookup&) = delete;
};
