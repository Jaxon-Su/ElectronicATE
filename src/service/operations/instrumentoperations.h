#pragma once
#include "instrumentactions.h"
#include "instrumentoperationresult.h"
#include "page1config.h"
#include "page2config.h"
#include <functional>

// Copyable callbacks are captured by workers; they must not depend on the ViewModel.
struct InstrumentOperations {
    using Result = InstrumentOperationResult;
    std::function<Result(const Page1Config &, const InputRow &, InputAction)> input;
    std::function<Result(const Page1Config &, const QVector<RelayDataRow> &, int, RelayAction)> relay;
    std::function<Result(const Page1Config &, const QVector<LoadDataRow> &, int, const LoadMetaRow &,
                         LoadAction, bool)>
        load;
    std::function<Result(const Page1Config &, const QVector<DynamicDataRow> &, int, const DynamicMetaRow &,
                         DynamicLoadAction, bool, bool)>
        dynamic;
    std::function<Result(const Page1Config &, int, const DcRow &, InputAction)> dcInput;
    std::function<Result(const Page1Config &, const DcGroup &, InputAction)> dcGroup;
};
