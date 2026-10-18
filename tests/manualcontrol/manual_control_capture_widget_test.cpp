#include "../support/configuration_wait.h"
#include "manualcontrolpage.h"
#include "triggermodelcatalog.h"
#include "messageservice.h"
#include <QApplication>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFontDatabase>
#include <QPushButton>
#include <QSettings>
#include <QScrollArea>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QThread>
#include <QTimer>
#include <iostream>
#include <stdexcept>

void require(bool ok, const char* message)
{
    if (!ok) throw std::runtime_error(message);
}

class CaptureScope : public Oscilloscope {
public:
    QString model() const override { return "OfflineScope"; }
    QString vendor() const override { return "Test"; }
    bool isConnected() const override { return true; }
    void disconnect() override {}
    int getTotalChannel() override { return channels; }
    QString getTriggerSource() override { ++sourceQueries; return "CH4"; }
    bool isChannelEnabled(int ch) override { return ch == 1 || ch == 3; }
    QByteArray captureScreenshot(const QString&, const QString&) override { ++screenshots; return "image"; }
    bool captureWaveformFileToHost(int ch, const QString& path, const QString& format,
                                   const QString&, int, int) override
    {
        capturedChannels.append(ch);
        formats.append(format);
        QFile file(path);
        return file.open(QIODevice::WriteOnly) && file.write("waveform") == 8;
    }
    int channels = 4, sourceQueries = 0, screenshots = 0;
    QList<int> capturedChannels;
    QStringList formats;
};

class SourceController : public ITriggerController {
public:
    explicit SourceController(QComboBox* source, QObject* parent) : ITriggerController(parent), combo(source) {}
    QString getSupportedModel() const override { return "MSOSeries456"; }
    void setInstrument(Oscilloscope* scope) override { instrument = scope; }
    Oscilloscope* getInstrument() const override { return instrument; }
    int getSelectedChannel() const override { ++sourceReads; return combo->currentData().toInt(); }
    QComboBox* combo;
    Oscilloscope* instrument = nullptr;
    mutable int sourceReads = 0;
};

// Isolate hardware/polling while exercising the real ManualControl -> ViewModel -> capture commands.
QWidget* TriggerWidgetFactory::createTriggerWidget(const QString& model, QWidget* parent,
                                                   QObject** controller, int channels)
{
    if (!TriggerModelCatalog::isSupported(model)) return nullptr;
    auto* group = new QGroupBox("Trigger", parent);
    auto* layout = new QVBoxLayout(group);
    auto* source = new QComboBox(group);
    source->setObjectName("testTriggerSource");
    for (int ch = 1; ch <= channels; ++ch) source->addItem(QString("CH%1").arg(ch), ch);
    layout->addWidget(source);
    *controller = new SourceController(source, group);
    return group;
}

void finish()
{
    require(QThreadPool::globalInstance()->waitForDone(3000), "background work timed out");
    QEventLoop loop;
    QTimer::singleShot(75, &loop, &QEventLoop::quit);
    loop.exec();
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setOrganizationName("ElectronicATEOffline");
    app.setApplicationName("ManualControlCaptureTest");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/arial.ttf");
    app.setFont(QFont("Arial", 9));
    try {
        QTemporaryDir dir;
        require(dir.isValid(), "temporary directory failed");
        auto scope = std::make_shared<CaptureScope>();
        ManualControlModel model;
        ManualControlOperations ops;
        ops.connectScopes = [scope](const InstrumentSetupConfig& config) {
            return OscilloscopeManager::OscMap{{config.instruments[0].modelName, scope}};
        };
        ManualControlViewModel vm(&model, ops);
        ManualControlPage page(&vm);
        auto* channel = page.findChild<QComboBox*>("cmbCaptureChannel");
        require(channel && !channel->isEnabled() && channel->count() == 0, "unconfigured selector enabled");
        auto configure = [&](const QString& name, bool enabled = true) {
            InstrumentSetupConfig config;
            InstrumentConfig instrument;
            instrument.type = "Oscilloscope";
            instrument.modelName = name;
            instrument.enabled = enabled;
            config.instruments.append(instrument);
            scope->channels = TriggerModelCatalog::channelCount(name);
            vm.onInstrumentSetupConfigChanged(config);
            startQueuedConfiguration(&vm);
            finish();
        };
        int warnings = 0, fileNumber = 0;
        QObject::connect(&MessageService::instance(), &MessageService::warningRequested, &app,
                         [&](const QString&, const QString&) { ++warnings; });
        bool changeDuringDialog = false;
        vm.setCaptureFileSelector([&](const QString&, const QString&, const QString& filter) {
            if (changeDuringDialog) channel->setCurrentIndex(0);
            const QString ext = filter.startsWith("CSV") ? "csv" : filter.startsWith("WFM") ? "wfm" : "png";
            return dir.filePath(QString::number(++fileNumber) + "." + ext);
        });
        auto click = [&](const char* name) {
            auto* button = page.findChild<QPushButton*>(name);
            require(button != nullptr, "capture button missing");
            require(button->isEnabled(), "capture button was not ready");
            button->click();
            finish();
            QElapsedTimer timer;
            timer.start();
            while ((vm.hasPendingControlOperation() || !button->isEnabled()) && timer.elapsed() < 3000) {
                QCoreApplication::processEvents();
                QThread::msleep(1);
            }
            require(!vm.hasPendingControlOperation() && button->isEnabled(), "capture completion did not unlock controls");
        };
        configure("MSO44B");
        require(channel->count() == 4 && channel->currentData().toInt() == 1, "four-channel default incorrect");
        auto* source = page.findChild<QComboBox*>("testTriggerSource");
        auto* controller = dynamic_cast<SourceController*>(source->parent()->findChild<ITriggerController*>());
        require(controller != nullptr, "test trigger controller missing");
        source->setCurrentIndex(3);
        channel->setCurrentIndex(1);
        click("btnCsv");
        require(scope->capturedChannels == QList<int>{2} && scope->formats.last() == "CSV", "CSV ignored Capture CH2");
        require(source->currentData().toInt() == 4, "Capture changed Trigger Source");
        source->setCurrentIndex(0);
        click("btnWfm");
        require(channel->currentData().toInt() == 2 && scope->capturedChannels.last() == 2
                    && scope->formats.last() == "WFM", "Trigger Source changed Capture WFM channel");
        channel->setCurrentIndex(2);
        changeDuringDialog = true;
        click("btnCsv");
        changeDuringDialog = false;
        require(scope->capturedChannels.last() == 3, "capture channel snapshot changed during file selection");
        channel->setCurrentIndex(3);
        const int transfers = scope->capturedChannels.size();
        click("btnPic");
        require(scope->screenshots == 1 && scope->capturedChannels.size() == transfers, "PNG became channel-specific");
        click("btnAllCsv");
        click("btnAllWfm");
        require(scope->capturedChannels.mid(transfers) == QList<int>({1, 3, 1, 3}), "All formats followed single channel");
        require(scope->sourceQueries == 0 && controller->sourceReads == 0, "capture read Trigger Source");
        configure("MSO44B");
        require(channel->currentData().toInt() == 4, "reconnect lost valid Capture selection");
        configure("MSO46B");
        require(channel->count() == 6 && channel->currentData().toInt() == 4, "six-channel model selection incorrect");
        channel->setCurrentIndex(5);
        click("btnWfm");
        require(scope->capturedChannels.last() == 6, "CH6 did not reach driver");
        configure("MSO58B");
        require(channel->count() == 8 && channel->currentData().toInt() == 6, "eight-channel model selection incorrect");
        channel->setCurrentIndex(7);
        click("btnCsv");
        require(scope->capturedChannels.last() == 8, "CH8 did not reach driver");
        configure("MSO44B");
        require(channel->count() == 4 && channel->currentData().toInt() == 1, "invalid old channel not reset to CH1");
        page.resize(1000, 900);
        page.show();
        app.processEvents();
        require(page.findChild<QGroupBox*>("grpCapture")->grab().save("manualControl-capture-controls.png"), "capture preview failed");
        auto* capture = page.findChild<QGroupBox*>("grpCapture");
        auto* inputScroll = page.findChild<QScrollArea*>("manualControlInputScroll");
        require(capture && inputScroll, "left-side controls missing");
        for (const QSize size : {QSize(640, 480), QSize(800, 600), QSize(1000, 900), QSize(640, 300)}) {
            page.resize(size);
            app.processEvents();
            require(page.size() == size, "layout forces a large minimum window");
            const QPoint fixedCapturePosition = capture->mapTo(&page, QPoint());
            for (int scrollPosition : {inputScroll->verticalScrollBar()->minimum(), inputScroll->verticalScrollBar()->maximum()}) {
                inputScroll->verticalScrollBar()->setValue(scrollPosition);
                app.processEvents();
                require(capture->mapTo(&page, QPoint()) == fixedCapturePosition, "Capture moves with input scrolling");
                require(page.rect().contains(QRect(fixedCapturePosition, capture->size())), "Capture outside page bounds");
                require(channel->visibleRegion().contains(channel->rect()), "Capture channel clipped");
                for (const char* name : {"btnPic", "btnCsv", "btnAllCsv", "btnWfm", "btnAllWfm"}) {
                    auto* button = page.findChild<QPushButton*>(name);
                    require(button && button->visibleRegion().contains(button->rect()), "Capture button clipped");
                }
            }
            if (size.height() <= 480) require(inputScroll->verticalScrollBar()->maximum() > 0, "input controls cannot scroll");
            if (size == QSize(640, 480)) require(page.grab().save("manualControl-capture-compact.png"), "compact preview failed");
        }
        channel->setCurrentIndex(2);
        click("btnWfm");
        require(scope->capturedChannels.last() == 3, "compact Capture changed channel dispatch");
        configure("MSO44B", false);
        require(channel->count() == 0 && !channel->isEnabled(), "disabled scope retained Capture choices");
        configure("UnsupportedScope");
        require(channel->count() == 0 && !channel->isEnabled(), "unsupported scope offered Capture channels");
        require(warnings == 0, "unexpected capture warning");
        QSettings().clear();
        std::cout << "PASS: independent Capture CH, CSV/WFM dispatch, snapshot, all formats and model channel counts\n";
    } catch (const std::exception& error) {
        QThreadPool::globalInstance()->waitForDone();
        std::cerr << error.what() << '\n';
        return 1;
    }
}
