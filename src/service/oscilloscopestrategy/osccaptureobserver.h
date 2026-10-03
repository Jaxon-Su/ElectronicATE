#pragma once
#include "scopecapturerecord.h"
#include <functional>

using OscCaptureObserver = std::function<void(const ScopeCaptureRecord &)>;
