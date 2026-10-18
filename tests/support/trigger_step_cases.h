#include "msoseries456.h"
#pragma once
#include "messageservice.h"
#include <QFontDatabase>
#include <QPushButton>
#include <QSignalBlocker>
#include <QThreadPool>
#include <cmath>
#include <limits>

class StepScope : public MSOSeries456 {
public:
    bool isConnected() const override { return connected; }
    int getTotalChannel() override { return 8; }
    bool isRunning() override { return false; }
    void setTriggerLevel(double) override { ++levelWrites; }
    double getChannelScale(int channel) override
    {
        ++reads;
        lastChannel = channel;
        onWorker = QThread::currentThread() != QCoreApplication::instance()->thread();
        if (slow) QThread::msleep(150);
        if (throwError) throw std::runtime_error("scale read failed");
        m_lastError = error;
        return scale;
    }
    double scale = 5.0;
    QString error;
    bool connected = true, slow = false, throwError = false, onWorker = false;
    int reads = 0, lastChannel = 0, levelWrites = 0;
};

static void testTriggerStep(bool destroy)
{
    auto scope = std::make_shared<StepScope>();
    auto widget = std::make_unique<MSOSeries456TriggerWidget>(8);
    auto* controller = qobject_cast<MSOSeries456TriggerController*>(widget->getTriggerController());
    for (auto* timer : controller->findChildren<QTimer*>()) timer->stop();
    controller->setInstrument(scope.get());
    controller->setPollingLeaseFactory([scope] { return scope; });
    auto* source = widget->findChild<QComboBox*>("triggerSource");
    auto* button = widget->findChild<QPushButton*>("triggerStepFromScale");
    auto* step = widget->findChild<SmartStepSpinBox*>("triggerLevelStep");
    auto* level = widget->findChild<SmartStepSpinBox*>("triggerLevel");
    require(button && step && level && source, "step controls missing");
    auto finish = [&] {
        require(QThreadPool::globalInstance()->waitForDone(3000), "step query timed out");
        QCoreApplication::processEvents();
    };
    {
        QSignalBlocker blocker(source);
        source->setCurrentIndex(2);
    }
    if (destroy) {
        scope->slow = true;
        button->click();
        require(controller->hasActiveCommand(), "step query did not start");
        std::weak_ptr<StepScope> weak = scope;
        widget.reset();
        scope.reset();
        require(!weak.expired(), "scope released while Step read is active");
        finish();
        require(weak.expired(), "Step query leaked scope after widget destruction");
        return;
    }
    int warnings = 0;
    QObject::connect(&MessageService::instance(), &MessageService::warningRequested, widget.get(),
                     [&](const QString&, const QString&) { ++warnings; });
    scope->slow = true;
    QElapsedTimer elapsed;
    elapsed.start();
    button->click();
    require(elapsed.elapsed() < 100 && controller->hasActiveCommand(), "Step query blocked UI");
    require(!button->isEnabled() && !step->isEnabled() && !source->isEnabled(), "Step query did not lock controls");
    button->click();
    bool responsive = false;
    QEventLoop loop;
    QTimer::singleShot(10, &loop, [&] { responsive = true; loop.quit(); });
    loop.exec();
    require(responsive && elapsed.elapsed() < 100, "Step query blocked event loop");
    finish();
    require(scope->reads == 1 && scope->lastChannel == 3 && scope->onWorker, "wrong source, duplicate read or UI-thread I/O");
    require(std::abs(step->value() - 0.2) < 1e-12 && button->isEnabled() && step->isEnabled(), "Scale / 25 result missing");
    level->setValue(0.0);
    level->stepUp();
    require(std::abs(level->value() - 0.2) < 1e-12 && scope->levelWrites == 0, "Step did not update Level increment or wrote instrument");
    scope->slow = false;
    for (double scale : {0.01, 0.001, 0.000001, 50.0}) {
        scope->scale = scale;
        button->click(); finish();
        require(std::abs(step->value() - scale / 25.0) < scale * 1e-10, "small Step was clamped or rounded");
        level->setValue(0.0); level->stepUp();
        require(std::abs(level->value() - scale / 25.0) < scale * 1e-10, "Level lost Step precision");
    }
    {
        QSignalBlocker blocker(source); source->setCurrentIndex(7);
    }
    button->click(); finish();
    require(scope->lastChannel == 8, "Step did not follow current Source CH8");
    const double previousStep = step->value();
    int expectedWarnings = 0;
    for (double scale : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(),
                         std::numeric_limits<double>::infinity(), 1e40, 1e-15}) {
        scope->scale = scale;
        button->click(); finish();
        require(warnings == ++expectedWarnings && step->value() == previousStep && button->isEnabled(),
                "invalid scale changed Step or did not report failure");
    }
    scope->scale = 1.0;
    scope->error = "transport failed";
    button->click(); finish();
    require(warnings == ++expectedWarnings && step->value() == previousStep, "transport error overwrote Step");
    scope->error.clear(); scope->throwError = true;
    button->click(); finish();
    require(warnings == ++expectedWarnings && step->value() == previousStep, "query exception overwrote Step");
    scope->throwError = false;
    int reads = scope->reads;
    controller->setSuspended(true);
    button->click(); finish();
    require(scope->reads == reads, "suspended Step accessed hardware");
    controller->setSuspended(false);
    for (auto* timer : controller->findChildren<QTimer*>()) timer->stop();
    scope->connected = false;
    button->click(); finish();
    require(scope->reads == reads && step->value() == previousStep, "disconnected Step changed state");
    scope->connected = true;
    scope->slow = true;
    button->click();
    { QSignalBlocker blocker(source); source->setCurrentIndex(0); }
    finish();
    require(step->value() == previousStep, "late result applied to a different Source");
    button->click();
    auto replacement = std::make_shared<StepScope>();
    controller->setInstrument(replacement.get());
    controller->setPollingLeaseFactory([replacement] { return replacement; });
    finish();
    require(step->value() == previousStep, "old instrument completion changed Step");
    controller->setInstrument(static_cast<Oscilloscope*>(nullptr));
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/arial.ttf");
    widget->setFont(QFont("Arial", 9));
    widget->resize(280, 640); widget->show(); QCoreApplication::processEvents();
    require(widget->grab().save("trigger-step-controls.png"), "Step preview failed");
    require(warnings == expectedWarnings, "unexpected Step warning");
}
