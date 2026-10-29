#pragma once
#include "../oscilloscopestrategy/osccaptureobserver.h"
#include "../measurementstrategy/imeasurementstrategy.h"
#include "ibrownoutio.h"
#include "brownoutsettings.h"

class IScopeBrownOut;

class BrownOutStrategy final : public IMeasurementStrategy {
  public:
    BrownOutStrategy(BrownOut::Settings settings, IScopeBrownOut &scope, BrownOutIoFactory createIo,
                     OscCaptureObserver capture = {}, std::function<void(const QString &)> log = {});
    QString name() const override;
    MeasurementResult execute(QAtomicInt &stop) override;

  private:
    BrownOut::Settings m_settings;
    IScopeBrownOut &m_scope;
    BrownOutIoFactory m_createIo;
    OscCaptureObserver m_capture;
    std::function<void(const QString &)> m_log;
};
