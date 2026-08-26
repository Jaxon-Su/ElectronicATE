#pragma once
#include <QString>
#include <QStringList>
#include "oscilloscope.h"
#include "icommunication.h"
#include "dpo7000.h"
#include "dpo4000.h"
#include "msoseries456.h"
// #include "dpo5000.h"  // 未來添加

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
    OscilloscopeFactory() = default;
};
