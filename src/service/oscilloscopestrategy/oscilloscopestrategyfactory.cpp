#include "oscilloscopestrategyfactory.h"
#include "normaltriggerstrategy.h"

IOscilloscopeMeasureStrategy* OscilloscopeStrategyFactory::create(const QString& taskName)
{
    if (taskName == "Static Test" || taskName == "Dynamic Test")
        return new NormalTriggerStrategy(/*timeoutMs=*/ 10000);

    // Turn On 使用 TurnOnRatchetStrategy，由 worker 直接 new（需注入依賴），
    // factory 不處理，此處保留 nullptr。
    return nullptr;
}
