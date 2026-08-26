#pragma once
#include "icapturecommand.h"
#include "capturecontext.h"

// 示波器 PNG/BMP 截圖命令
class PngCaptureCommand : public ICaptureCommand {
public:
    explicit PngCaptureCommand(const CaptureContext& ctx);
    void execute() override;

private:
    CaptureContext m_ctx;
};
