#pragma once
#include "oscwritedialogbase.h"

class Oscilloscope;
class QWidget;

// MSO 4/5/6 uses its settings dialog. Missing or unsupported models receive
// a Close-only unavailable dialog. No instrument is created by this factory.
OscWriteDialogBase* createWriteOscilloscopeDialog(Oscilloscope* scope, const QString& configuredModel,
                                                  const QVariantMap& initCfg, int seqNo,
                                                  const QString& extName, QWidget* parent = nullptr);
