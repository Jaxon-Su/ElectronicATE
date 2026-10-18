#pragma once
#include "oscilloscopeacquisition.h"

namespace SteadyDetail
{
// One Static/Dynamic measurement. No input/load control and no destructor I/O.
class Search
{
  public:
    Search(IScopeMeasurement *scope, QAtomicInt &stop, const QString &target,
           int timeoutMs, int phaseTimeoutMs, int maxIterations, const OscCaptureObserver &capture);
    OscMeasureResult execute();

  private:
    bool initializeChannels();
    bool captureReference();
    bool searchDirection(bool maximum);
    bool accumulateRecord(bool maximum, int step, double level, double &peak);
    OscMeasureResult finish();

    IScopeMeasurement *scope;
    QAtomicInt &stopFlag;
    const OscCaptureObserver &captureObserver;
    const QString m_target;
    const int m_timeoutMs, m_phaseTimeoutMs, m_maxIterations;
    OscMeasureResult result;
    QMap<int, OscChannelMeasure> channels;
    OscilloscopeAcquisition::VerticalScaleState vertical;
    int triggerChannel = 0;
    double initialMax = 0, initialMin = 0;
};
} // namespace SteadyDetail
