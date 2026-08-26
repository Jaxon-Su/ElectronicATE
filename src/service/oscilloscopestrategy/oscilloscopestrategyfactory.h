#pragma once
#include <QString>

class IOscilloscopeMeasureStrategy;

// ══════════════════════════════════════════════════════
//  OscilloscopeStrategyFactory
//
//  依 task 名稱建立對應的量測策略。
//  回傳 new 出的物件，呼叫端負責 delete（或用 unique_ptr）。
//  不需要示波器的 task 回傳 nullptr。
// ══════════════════════════════════════════════════════
class OscilloscopeStrategyFactory
{
public:
    static IOscilloscopeMeasureStrategy* create(const QString& taskName);
};
