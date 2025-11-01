// InstrumentBase.h
#pragma once
#include <QString>

// 抽象基底類
class InstrumentBase
{
public:
    virtual ~InstrumentBase() = default;

    // 查詢型號、廠牌
    virtual QString model() const = 0;
    virtual QString vendor() const = 0;

};
