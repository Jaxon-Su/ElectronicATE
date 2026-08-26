#pragma once
#include <QString>
#include "acsource.h"
#include "icommunication.h"

// 工廠類別
class ACSourceFactory
{
public:
    // 靜態工廠方法
    static ACSource* createACSource(const QString& modelName, ICommunication* comm);
};
