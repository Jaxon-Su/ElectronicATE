#pragma once

#include "icommunication.h"
#include <QString>


class CommunicationFactory
{
public:
    // 解析 resource，回傳對應協議物件（用戶要記得 delete）
    static ICommunication* create(const QString& resource);
};
