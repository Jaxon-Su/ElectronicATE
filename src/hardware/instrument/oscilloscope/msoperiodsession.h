#pragma once
#include "iscopeperiodsession.h"
#include <memory>
class Oscilloscope;
std::unique_ptr<IScopePeriodSession>
makeMsoPeriodSession(Oscilloscope &scope, IScopePeriodSession::Continue ready, QString &error);
