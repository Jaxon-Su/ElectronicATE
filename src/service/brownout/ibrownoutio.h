#pragma once
#include <QString>
#include <functional>
#include <memory>

// Created, used and destroyed on the voltage-control thread. No shared transport with the scope.
class IBrownOutIo {
  public:
    virtual ~IBrownOutIo() = default;
    virtual void start(const std::function<bool()> &cancelled) = 0;
    virtual double initialVoltage() const = 0;
    virtual void setVoltage(double voltage) = 0;
    virtual double outputVoltage() = 0;
    virtual bool shutdown(QString &error) = 0;
};

using BrownOutIoFactory = std::function<std::unique_ptr<IBrownOutIo>()>;
