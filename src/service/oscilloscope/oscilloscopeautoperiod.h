#pragma once
#include "../oscilloscopestrategy/osccaptureobserver.h"
#include "../oscilloscopestrategy/verticalscaleoptions.h"
#include <QAtomicInt>
#include <QString>
class IScopeAutoPeriod;
// Caller owns exclusive access. Target five cycles; accept three through eight.
bool adjustOscilloscopeAutoPeriod(IScopeAutoPeriod* scope, QAtomicInt& stop, QString& error,
                                  const OscCaptureObserver &capture = {}, const VerticalScaleOptions &options = {});
