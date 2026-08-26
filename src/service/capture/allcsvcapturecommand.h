#pragma once
#include "icapturecommand.h"
#include "capturecontext.h"

// 示波器全通道波形 CSV 命令（每個 enabled 通道輸出一個檔案）
class AllCsvCaptureCommand : public ICaptureCommand {
public:
    explicit AllCsvCaptureCommand(const CaptureContext& ctx);
    void execute() override;

private:
    CaptureContext m_ctx;
};
