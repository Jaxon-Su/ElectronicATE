#pragma once

// 所有擷取命令的抽象介面
// 任何 ViewModel 只需組裝 CaptureContext 並呼叫 execute()，
// 不需知道 PNG / CSV / AllCSV 的實作細節。
class ICaptureCommand {
public:
    virtual ~ICaptureCommand() = default;
    // Every command enters through the same synchronous exception boundary.
    virtual void execute() final;

protected:
    virtual void executeImpl() = 0;
};
