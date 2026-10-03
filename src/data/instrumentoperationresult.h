#pragma once
#include <QString>
#include <optional>

struct InstrumentOperationResult {
    bool success = true;
    QString errorMessage;
    // True only when the executor knows it did not change the output state.
    bool outputUnchanged = false;
    std::optional<bool> confirmedOutput;
};
