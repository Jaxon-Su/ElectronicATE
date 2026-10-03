#pragma once
#include "capturecontext.h"

enum class WaveformCaptureFormat { Csv, Wfm };

// Shared single-channel entry, including synchronous preparation error handling.
// Instrument-specific transfer stays in the driver.
void executeWaveformCapture(const CaptureContext& context, WaveformCaptureFormat format);
