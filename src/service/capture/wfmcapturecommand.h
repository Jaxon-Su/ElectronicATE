#pragma once
#include "icapturecommand.h"
#include "capturecontext.h"

// 示波器單通道波形 WFM 命令（使用 Capture 選取的通道）
// 使用 INTERNal 格式，儲存 Tektronix 原生二進制 .wfm 檔
class WfmCaptureCommand : public ICaptureCommand {
  public:
    explicit WfmCaptureCommand(const CaptureContext& ctx);

  private:
    void executeImpl() override;
    CaptureContext m_ctx;
};
