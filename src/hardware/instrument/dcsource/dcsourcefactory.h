#pragma once
#include <QString>
#include "dcsource.h"
#include "icommunication.h"

// 工廠類別
class DCSourceFactory
{
public:
    // 靜態工廠方法
    static DCSource* createDCSource(const QString& modelName, ICommunication* comm);
};
