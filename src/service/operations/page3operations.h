#pragma once
#include "instrumentactions.h"
#include "instrumentoperationresult.h"
#include "page1config.h"
#include "page2config.h"
#include "oscilloscopemanager.h"
#include <functional>

// Copyable callbacks are captured by workers; they must not depend on the ViewModel.
struct Page3Operations {
    using Result = InstrumentOperationResult;
    std::function<Result(const Page1Config&, const InputRow&, InputAction)> input;
    std::function<Result(const Page1Config&, const QVector<RelayDataRow>&, int, RelayAction)> relay;
    std::function<Result(const Page1Config&, const QVector<LoadDataRow>&, int, const LoadMetaRow&, LoadAction, bool)> load;
    std::function<Result(const Page1Config&, const QVector<DynamicDataRow>&, int, const DynamicMetaRow&, DyLoadAction, bool, bool)> dynamic;
    std::function<OscilloscopeManager::OscMap(const Page1Config&)> connectScopes;
};
