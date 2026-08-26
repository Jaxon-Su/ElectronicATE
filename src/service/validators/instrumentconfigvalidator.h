#pragma once

#include <QString>
#include "page1config.h"

/**
 * @brief 儀器配置驗證器（通用版本）
 *
 * 使用通用驗證方法，減少代碼重複
 *
 * 使用範例：
 * @code
 * auto result = InstrumentConfigValidator::validateInput(config, selectedText);
 * if (!result.isValid) {
 *     MessageService::instance().showWarning(result.errorTitle, result.errorMessage);
 *     return;
 * }
 * @endcode
 */
class InstrumentConfigValidator {
public:
    /**
     * @brief 驗證結果結構
     */
    struct ValidationResult {
        bool isValid = false;       ///< 是否有效
        QString errorTitle;          ///< 錯誤標題（用於對話框）
        QString errorMessage;        ///< 錯誤訊息（用於對話框）
    };

    /**
     * @brief 驗證 Input 配置
     * @param config Page1 配置
     * @param selectedText 選擇的文本（如 "110V/60Hz/0°"）
     * @return 驗證結果
     */
    static ValidationResult validateInput(
        const Page1Config& config,
        const QString& selectedText
        );

    /**
     * @brief 驗證 Load 配置
     * @param config Page1 配置
     * @param selectedText 選擇的文本
     * @return 驗證結果
     */
    static ValidationResult validateLoad(
        const Page1Config& config,
        const QString& selectedText
        );

    /**
     * @brief 驗證 Dynamic Load 配置
     * @param config Page1 配置
     * @param selectedText 選擇的文本
     * @return 驗證結果
     */
    static ValidationResult validateDyLoad(
        const Page1Config& config,
        const QString& selectedText
        );

    /**
     * @brief 驗證 Relay 配置
     * @param config Page1 配置
     * @param selectedText 選擇的文本
     * @return 驗證結果
     */
    static ValidationResult validateRelay(
        const Page1Config& config,
        const QString& selectedText
        );

private:
    /**
     * @brief 通用驗證方法（核心實現）
     *
     * @param config Page1 配置
     * @param selectedText 選擇的文本
     * @param instrumentType 儀器類型（用於檢查，如 "Input", "Load", "Relay"）
     * @param displayName 顯示名稱（用於錯誤訊息，如 "input", "load", "relay"）
     * @return 驗證結果
     *
     * @note 此方法為私有方法，僅供內部使用
     *       所有公開的驗證方法都調用此方法
     */
    static ValidationResult validate(
        const Page1Config& config,
        const QString& selectedText,
        const QString& instrumentType,
        const QString& displayName
        );

    // 禁止實例化
    InstrumentConfigValidator() = delete;
    ~InstrumentConfigValidator() = delete;
    InstrumentConfigValidator(const InstrumentConfigValidator&) = delete;
    InstrumentConfigValidator& operator=(const InstrumentConfigValidator&) = delete;
};
