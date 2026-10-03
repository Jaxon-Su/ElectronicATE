#pragma once
#include "iscopecapture.h"
#include "binaryfilestore.h"

// Both manual capture and queued tasks use the same driver transfer and atomic save.
namespace CaptureFile {
inline bool screenshot(IScopeCapture* scope, const QString& path, const QString& format)
{
    const auto bytes = scope->captureScreenshot(format, path);
    return !bytes.isEmpty() && BinaryFileStore::save(path, bytes).succeeded();
}
inline bool waveform(IScopeCapture* scope, int channel, const QString& path, const QString& format)
{
    const auto temp = QString("C:\\TekScope\\Waveforms\\wave_ch%1.%2").arg(channel).arg(format.toLower());
    return scope->captureWaveformFileToHost(channel, path, format, temp);
}
} // namespace CaptureFile
