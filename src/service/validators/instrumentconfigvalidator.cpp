#include "instrumentconfigvalidator.h"

// ========================================
// 通用驗證方法（核心實現）
// ========================================

InstrumentConfigValidator::ValidationResult
InstrumentConfigValidator::validate(
    const Page1Config& config,
    const QString& selectedText,
    const QString& instrumentType,
    const QString& displayName)
{
    ValidationResult result;
    result.isValid = false;

    // ===== 檢查 1: 配置是否為空 =====
    if (config.instruments.isEmpty()) {
        result.errorTitle = "Error Message";
        result.errorMessage =
            "No instrument settings have been loaded.\n"
            "Please load the configuration first!";
        return result;
    }

    // ===== 檢查 2: 是否有選擇 =====
    if (selectedText.trimmed().isEmpty()) {
        result.errorTitle = "Error Message";
        result.errorMessage = QString(
                                  "No %1 conditions selected.\n"
                                  "Please select %1 conditions first!"
                                  ).arg(displayName);  // 動態插入顯示名稱
        return result;
    }

    // ===== 檢查 3: 是否有啟用的指定類型儀器 =====
    bool hasEnabledInstrument = false;
    for (const auto& inst : config.instruments) {
        if (inst.type == instrumentType && inst.enabled) {
            hasEnabledInstrument = true;
            break;
        }
    }

    if (!hasEnabledInstrument) {
        result.errorTitle = "Error Message";
        result.errorMessage = QString(
                                  "No %1 instruments enabled in configuration.\n"
                                  "Please enable at least one %1 instrument!"
                                  ).arg(instrumentType);  // 動態插入儀器類型
        return result;
    }

    // ===== 所有檢查通過 =====
    result.isValid = true;
    return result;
}

// ========================================
// 公開的專用驗證方法（調用通用方法）
// ========================================

InstrumentConfigValidator::ValidationResult
InstrumentConfigValidator::validateInput(
    const Page1Config& config,
    const QString& selectedText)
{
    // 調用通用方法
    // instrumentType: "Input" - 用於檢查 inst.type
    // displayName: "input" - 用於錯誤訊息
    return validate(config, selectedText, "InputSource", "input");
}

InstrumentConfigValidator::ValidationResult
InstrumentConfigValidator::validateLoad(
    const Page1Config& config,
    const QString& selectedText)
{
    // 調用通用方法
    // instrumentType: "Load" - 用於檢查 inst.type
    // displayName: "load" - 用於錯誤訊息
    return validate(config, selectedText, "Load", "load");
}

InstrumentConfigValidator::ValidationResult
InstrumentConfigValidator::validateDyLoad(
    const Page1Config& config,
    const QString& selectedText)
{
    // 調用通用方法
    // ⚠️ 注意：instrumentType 還是 "Load"
    //    因為 DyLoad 和 Load 使用同一台儀器
    // displayName: "dynamic load" - 用於錯誤訊息
    return validate(config, selectedText, "Load", "dynamic load");
}

InstrumentConfigValidator::ValidationResult
InstrumentConfigValidator::validateRelay(
    const Page1Config& config,
    const QString& selectedText)
{
    // 調用通用方法
    // instrumentType: "Relay" - 用於檢查 inst.type
    // displayName: "relay" - 用於錯誤訊息
    return validate(config, selectedText, "Relay", "relay");
}
