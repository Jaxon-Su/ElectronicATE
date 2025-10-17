#pragma once
#include <QString>
#include "acsource.h"
#include "deltaa3000.h"
#include "icommunication.h"

// 工廠類別
class ACSourceFactory
{
public:
    ACSourceFactory(); // 可以省略實作

    // 靜態工廠方法
    static ACSource* createACSource(const QString& modelName, ICommunication* comm);
};
