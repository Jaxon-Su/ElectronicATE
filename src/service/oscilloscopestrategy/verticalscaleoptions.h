#pragma once
#include <functional>
#include <QString>

struct VerticalScaleOptions {
    bool margin = false;
    // Called before a write, including writes that may partially fail. The owner
    // retains the first scale for each channel until the whole task finishes.
    std::function<void(int channel, double previousScale, double requestedScale)> beforeChange;
    std::function<void(int channel, double previousPosition, double requestedPosition)> beforePositionChange;
    std::function<void(const QString &)> log;
};
