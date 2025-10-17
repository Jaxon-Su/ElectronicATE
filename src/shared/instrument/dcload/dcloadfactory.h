#pragma once
#include <QString>
#include "dcload.h"
#include "icommunication.h"

// 工廠類別
class DCLoadFactory
{
public:
    DCLoadFactory(); // 可以省略實作

    // 靜態工廠方法
    static DCLoad* createDCLoad(const QString& modelName, ICommunication* comm);

};
