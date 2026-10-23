#include "oscilloscopestrategyfactory.h"
#include "steadystatesearchstrategy.h"
#include "turnonstrategy.h"
#include "turnoffstrategy.h"
#include "turnonthenshortstrategy.h"
#include "shortthenturnonstrategy.h"
#include "descentstrategy.h"
IOscilloscopeMeasureStrategy* OscilloscopeStrategyFactory::create(const QString& taskName,
                                                                  const TransientContext* context)
{
    if (taskName == "Static Test" || taskName == "Dynamic Test")
        return new SteadyStateSearchStrategy(10000, 900000, 100, context ? context->target : "BOTH",
                                             context ? context->verticalScale : VerticalScaleOptions{});
    if (!context)
        return nullptr;
    if (taskName == "Turn off (descent)")
        return new DescentStrategy(false, *context);
    if (taskName == "Turn on then short (descent)")
        return new DescentStrategy(true, *context);
    if (taskName == "Turn on")
        return new TurnOnStrategy(*context);
    if (taskName == "Turn off")
        return new TurnOffStrategy(*context);
    if (taskName == "Turn on then short")
        return new TurnOnThenShortStrategy(*context);
    if (taskName == "Short then turn on")
        return new ShortThenTurnOnStrategy(*context);
    return nullptr;
}
