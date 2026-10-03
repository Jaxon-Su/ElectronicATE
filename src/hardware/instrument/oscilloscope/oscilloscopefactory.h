#pragma once
#include <QString>
#include <QStringList>
#include "oscilloscope.h"
#include "icommunication.h"
#include "msoseries456.h"

class OscilloscopeFactory {
  public:
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
