#pragma once
#include "../oscilloscopestrategy/osccaptureobserver.h"
#include "brownoutsettings.h"
#include "measurementresult.h"
#include "iscopebrownout.h"
#include "ibrownoutio.h"
#include <QAtomicInt>
MeasurementResult runBrownOut(const BrownOut::Settings &settings, IScopeBrownOut &scope,
                          const BrownOutIoFactory &createIo, QAtomicInt &stop, const OscCaptureObserver &capture = {});
