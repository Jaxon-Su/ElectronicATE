#pragma once
#include "icapturecommand.h"
#include "capturecontext.h"

// 示波器單通道波形 CSV 命令（通道由 Trigger Source 決定）
class CsvCaptureCommand : public ICaptureCommand {
public:
    explicit CsvCaptureCommand(const CaptureContext& ctx);


private:
    void executeImpl() override;
    CaptureContext m_ctx;
};
