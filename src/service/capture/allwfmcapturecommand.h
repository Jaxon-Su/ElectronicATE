#pragma once
#include "icapturecommand.h"
#include "capturecontext.h"

// 示波器全通道波形 WFM 命令（每個 enabled 通道輸出一個檔案）
class AllWfmCaptureCommand : public ICaptureCommand {
public:
    explicit AllWfmCaptureCommand(const CaptureContext& ctx);


private:
    void executeImpl() override;
    CaptureContext m_ctx;
};
