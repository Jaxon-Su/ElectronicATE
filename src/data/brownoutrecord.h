#pragma once
#include "channelmeasurement.h"
#include <QVector>

enum class BrownOutRecordStatus { Pending, Ready, Error };

struct BrownOutRecord {
    // Monotonically increasing across the reference and event records of one session.
    quint64 sequence = 0;
    QVector<ChannelMeasurement> channels;
    bool clipped = false;
};
