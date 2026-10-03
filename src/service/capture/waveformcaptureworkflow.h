#pragma once
#include "capturecontext.h"

enum class WaveformCaptureFormat { Csv, Wfm };

// Shared single-channel workflow; instrument-specific transfer stays in the driver.
void executeWaveformCapture(const CaptureContext& context, WaveformCaptureFormat format);
