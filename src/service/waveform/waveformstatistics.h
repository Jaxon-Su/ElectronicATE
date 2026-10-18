#pragma once

#include "../../data/waveformdata.h"
#include <atomic>
#include <memory>

namespace Waveform {

using Cancellation = std::shared_ptr<std::atomic_bool>;
bool stopped(const Cancellation &cancel);
Result summarize(Trace trace, const Cancellation &cancel = {});

} // namespace Waveform
