#pragma once
#include "icapturecommand.h"
#include "capturecontext.h"

// 示波器單通道波形 CSV 命令（使用 Capture 選取的通道）
class CsvCaptureCommand : public ICaptureCommand {
  public:
    explicit CsvCaptureCommand(const CaptureContext& ctx);

  private:
    void executeImpl() override;
    CaptureContext m_ctx;
};
