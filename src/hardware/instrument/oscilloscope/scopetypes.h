#pragma once

enum class ScopeAcquisitionState { Invalid, Running, Completed };
// Ready can accept the external event; Arming is still filling the pre-trigger record.
enum class ScopeTriggerState { Invalid, Arming, Ready, Triggered, Automatic, Saving };
enum class ScopeTriggerSlope { Rise, Fall };
enum class ScopeTriggerMode { Auto, Normal };
enum class ScopeReferenceStatus { Pending, Completed, Error };
enum class ScopeMeasurement { Maximum, Minimum, Rms, Mean };

// Acquisition window in the same physical units as channel measurements.
// Display position is deliberately excluded.
struct ScopeVerticalRange {
    double scale = 0;
    double center = 0;
    double divisions = 0;
};
