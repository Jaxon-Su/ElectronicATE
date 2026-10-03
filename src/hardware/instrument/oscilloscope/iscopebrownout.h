#pragma once
#include "brownoutsettings.h"
#include "brownoutrecord.h"
#include <memory>
#include <QAtomicInt>

class IScopeBrownOutSession {
  public:
    virtual ~IScopeBrownOutSession() = default;
    virtual bool arm(QString &error) = 0;
    virtual bool readyToRamp(QString &error) = 0;
    // Error may carry a fresh, frozen record's sequence/channels for diagnostic
    // Capture. Empty record means completion/identity was not established.
    virtual BrownOutRecordStatus poll(BrownOutRecord &record, QString &error) = 0;
    virtual bool advanceLevel(double level, QString &error) = 0;
    virtual double levelStep() const = 0;
    virtual int recordDurationMs() const = 0;
    virtual bool finish(QString &error) = 0;
};
class IScopeBrownOut {
  public:
    virtual ~IScopeBrownOut() = default;
    virtual std::unique_ptr<IScopeBrownOutSession> beginBrownOut(const BrownOut::Settings &, QAtomicInt &stop,
                                                              QString &error) = 0;
};
