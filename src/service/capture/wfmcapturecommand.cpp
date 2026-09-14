#include "wfmcapturecommand.h"
#include "waveformcaptureworkflow.h"

WfmCaptureCommand::WfmCaptureCommand(const CaptureContext& ctx)
    : m_ctx(ctx)
{}

void WfmCaptureCommand::executeImpl()
{
    executeWaveformCapture(m_ctx, WaveformCaptureFormat::Wfm);
}
