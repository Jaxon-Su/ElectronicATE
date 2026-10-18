#pragma once

#include "../../service/waveform/waveformstatistics.h"

namespace Waveform {
Result readTekWfm(const QString &path, const Cancellation &cancel = {});
}
