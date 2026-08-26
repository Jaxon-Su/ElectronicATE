#pragma once
#include "icapturecommand.h"
#include "capturecontext.h"

// 示波器單通道波形 WFM 命令（通道由 Trigger Source 決定）
// 使用 INTERNal 格式，儲存 Tektronix 原生二進制 .wfm 檔
class WfmCaptureCommand : public ICaptureCommand {
public:
    explicit WfmCaptureCommand(const CaptureContext& ctx);
    void execute() override;

private:
    CaptureContext m_ctx;
};
