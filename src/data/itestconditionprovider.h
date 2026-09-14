#pragma once

#include <QVector>
#include "page2config.h"

// Read-only access to live test conditions, independent of any page or widget.
// The provider is borrowed: it must outlive its consumer, or be detached first.
// Access remains on the owning UI thread; workers receive execution snapshots.
class ITestConditionProvider {
public:
    virtual ~ITestConditionProvider() = default;
    virtual const QVector<InputRow>& inputRows() const = 0;
    virtual const LoadMetaRow& loadMeta() const = 0;
    virtual const QVector<LoadDataRow>& loadRows() const = 0;
    virtual const DynamicMetaRow& dynamicMeta() const = 0;
    virtual const QVector<DynamicDataRow>& dynamicRows() const = 0;
    virtual const QVector<RelayDataRow>& relayRows() const = 0;
};
