#pragma once
#include "instrumentactions.h"
#include "instrumentoperationresult.h"
#include "instrumentsetupconfig.h"
#include "testconditionsconfig.h"
#include <functional>

// Copyable callbacks are captured by workers; they must not depend on the ViewModel.
struct InstrumentOperations {
    using Result = InstrumentOperationResult;
    std::function<Result(const InstrumentSetupConfig &, const InputRow &, InputAction)> input;
    std::function<Result(const InstrumentSetupConfig &, const QVector<RelayDataRow> &, int, RelayAction)> relay;
    std::function<Result(const InstrumentSetupConfig &, const QVector<LoadDataRow> &, int, const LoadMetaRow &,
                         LoadAction, bool)>
        load;
    std::function<Result(const InstrumentSetupConfig &, const QVector<DynamicDataRow> &, int, const DynamicMetaRow &,
                         DynamicLoadAction, bool, bool)>
        dynamic;
    std::function<Result(const InstrumentSetupConfig &, int, const DcRow &, InputAction)> dcInput;
    std::function<Result(const InstrumentSetupConfig &, const DcGroup &, InputAction)> dcGroup;
};
