#pragma once
#include <functional>

// Submit owned work to the application thread pool. Capture resources by value;
// the callable (and its CaptureLease) lives until completion, including errors.
// Failures are queued to the application thread through MessageService.
void runCaptureTask(std::function<void()> task);
