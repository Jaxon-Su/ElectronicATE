#pragma once
#include <functional>
#include <QString>
#include <QMap>
#include <memory>

struct ChannelFitLimits {
    double clippedScale = 0;
    // Pairs hold requested and observed V/div respectively.
    QPair<double,double> roundedUp;
    QPair<double,double> roundedDown;
};
using VerticalFitHistory = QMap<int, ChannelFitLimits>;

struct VerticalScaleOptions {
    bool margin = false;
    // Notify before even a partially applied write so the owner can ensure STOP.
    std::function<void(int channel, double previousScale, double requestedScale)> beforeChange;
    std::function<void(int channel, double previousPosition, double requestedPosition)> beforePositionChange;
    std::function<void(const QString &)> log;
    std::shared_ptr<VerticalFitHistory> history;
};
