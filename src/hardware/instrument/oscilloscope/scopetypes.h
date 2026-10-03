#pragma once

enum class ScopeAcquisitionState { Invalid, Running, Completed };
// Ready can accept the external event; Arming is still filling the pre-trigger record.
enum class ScopeTriggerState { Invalid, Arming, Ready, Triggered, Automatic, Saving };
enum class ScopeTriggerSlope { Rise, Fall };
enum class ScopeTriggerMode { Auto, Normal };
enum class ScopeMeasurement { Maximum, Minimum, Rms, Mean };
