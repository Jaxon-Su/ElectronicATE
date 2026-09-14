#include "csvcapturecommand.h"
#include "waveformcaptureworkflow.h"

CsvCaptureCommand::CsvCaptureCommand(const CaptureContext& ctx)
    : m_ctx(ctx)
{}

void CsvCaptureCommand::executeImpl()
{
    executeWaveformCapture(m_ctx, WaveformCaptureFormat::Csv);
}
