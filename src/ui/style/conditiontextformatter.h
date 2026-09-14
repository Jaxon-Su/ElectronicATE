#pragma once

#include "page2config.h"

// Shared presentation text; no dependency on a page, ViewModel, or widget.
namespace ConditionTextFormatter {

inline QString inputTitle(const InputRow& row)
{
    if (row.phaseMode.isEmpty() || row.vin.isEmpty() || row.frequency.isEmpty() || row.phase.isEmpty())
        return {};

    return QString("%1/%2/%3/%4").arg(row.phaseMode, row.vin, row.frequency, row.phase);
}

}
