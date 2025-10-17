#pragma once
#include <QString>
#include <QStringList>
#include "Oscilloscope.h"
#include "icommunication.h"
#include "dpo7000.h"
// #include "dpo5000.h"  // 未來添加
// #include "mso4000.h"  // 未來添加

class OscilloscopeFactory
{
public:
    // 靜態工廠方法
    static Oscilloscope* createOscilloscope(const QString& modelName, ICommunication* comm);

    // 獲取支持的型號列表
    static QStringList getSupportedModels();

    // 檢查是否支持某型號
    static bool isModelSupported(const QString& modelName);

    // 獲取廠商信息
    static QString getVendorByModel(const QString& modelName);

private:
    // 私有構造函數，防止實例化
    OscilloscopeFactory() = default;
};
