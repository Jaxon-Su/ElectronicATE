#pragma once
#include <QString>
#include <functional>

// One temporary PERIOD measurement. The caller retains exclusive scope
// ownership.
class IScopePeriodSession
{
  public:
    using Continue = std::function<bool()>;
    virtual ~IScopePeriodSession() = default;
    virtual double horizontalDivisions() const = 0;
    virtual bool readTimeScale(double &secondsPerDivision, QString &error) = 0;
    virtual bool setTimeScale(double secondsPerDivision, QString &error) = 0;
    virtual bool prepareRecord(QString &error) = 0;
    virtual bool readPeriod(double &seconds, QString &error) = 0;
    // Always removes temporary state. Resume only after successful, uncancelled
    // adjustment.
    virtual bool restore(bool resume, QString &error) = 0;
};
