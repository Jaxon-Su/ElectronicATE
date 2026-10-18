#pragma once

#include "../../service/waveform/waveformstatistics.h"

namespace Waveform {
// Successful return means the atomic replacement has committed; cancellation after that cannot undo it.
Result exportCsv(const QString &path, const Trace &trace, const Cancellation &cancel = {});
}
