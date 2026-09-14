#include "csvcapturecommand.h"
#include "wfmcapturecommand.h"
#include "allcsvcapturecommand.h"
#include "oscilloscope.h"
#include "messageservice.h"
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <QThread>
#include <QThreadPool>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

class WaveformScope : public Oscilloscope {
public:
    QString model() const override { return "OfflineScope"; }
    QString vendor() const override { return "Test"; }
    bool isConnected() const override { return true; }
    QByteArray captureScreenshot(const QString&, const QString&) override { return {}; }
    QString getTriggerSource() override { ++queries; return source; }
    int getTotalChannel() override { return 4; }
    bool isChannelEnabled(int channel) override { return channel == 1 || channel == 3; }
    bool captureWaveformFileToHost(int channel, const QString& host, const QString& format,
                                   const QString& remote, int start, int stop) override
    {
        ++transfers;
        lastChannel = channel;
        lastFormat = format;
        lastRemote = remote;
        hostPaths.append(host);
        ranOnWorker = QThread::currentThread() != QCoreApplication::instance()->thread();
        require(start == -1 && stop == -1, "sample bounds changed");
        if (throwUnknown) throw 42;
        if (throwError) throw std::runtime_error("offline transfer error");
        if (!succeed || channel == failChannel) return false;
        QFile file(host);
        return file.open(QIODevice::WriteOnly) && file.write("waveform") == 8;
    }
    QString source = "CH3";
    int queries = 0;
    int transfers = 0;
    int lastChannel = 0;
    QString lastFormat, lastRemote;
    QStringList hostPaths;
    bool succeed = true;
    bool throwError = false;
    bool throwUnknown = false;
    bool ranOnWorker = false;
    int failChannel = 0;
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        QTemporaryDir dir;
        require(dir.isValid(), "temporary directory failed");
        int infos = 0, warnings = 0;
        QString lastMessage;
        QObject::connect(&MessageService::instance(), &MessageService::infoRequested, &app,
                         [&](const QString&, const QString& message) {
            require(QThread::currentThread() == app.thread(), "notification outside application thread");
            ++infos;
            lastMessage = message;
        });
        QObject::connect(&MessageService::instance(), &MessageService::warningRequested, &app,
                         [&](const QString&, const QString& message) { ++warnings; lastMessage = message; });

        for (bool csv : {true, false}) {
            auto scope = std::make_shared<WaveformScope>();
            CaptureContext context;
            context.oscilloscope = scope;
            context.captureSession = std::make_shared<CaptureSession>();
            const QString extension = csv ? "csv" : "wfm";
            const QString path = dir.filePath("wave." + extension);
            int remembered = 0;
            context.onSaveDirChanged = [&](const QString& selected) {
                require(selected == path, "wrong remembered path");
                ++remembered;
            };
            context.selectSaveFile = [&](const QString& title, const QString& suggested, const QString& filter) {
                require(title == (csv ? "Save Waveform Data" : "Save Waveform Data (WFM)"), "dialog title changed");
                require(suggested.endsWith("." + extension) && filter.contains("*." + extension), "format metadata changed");
                return path;
            };
            auto run = [&] {
                if (csv) CsvCaptureCommand(context).execute();
                else WfmCaptureCommand(context).execute();
                require(QThreadPool::globalInstance()->waitForDone(3000), "capture did not finish");
                QCoreApplication::processEvents();
                require(!context.captureSession->isBusy(), "capture lease leaked");
            };
            const int originalInfos = infos, originalWarnings = warnings;
            context.captureChannel = 2;
            run();
            require(scope->queries == 0 && scope->lastChannel == 2, "UI channel did not bypass query");
            require(scope->lastFormat == extension.toUpper()
                        && scope->lastRemote == "C:\\TekScope\\Waveforms\\wave_ch2." + extension,
                    "driver transfer arguments changed");
            require(infos == originalInfos + 1 && lastMessage.contains("CH2") && lastMessage.contains(path),
                    "success notification missing");
            context.captureChannel = 0;
            run();
            require(scope->queries == 1 && scope->lastChannel == 3, "trigger source fallback changed");
            scope->source = "AUX";
            run();
            require(scope->lastChannel == 1, "invalid source fallback changed");
            scope->succeed = false;
            run();
            require(warnings == originalWarnings + 1 && infos == originalInfos + 3,
                    "failed transfer reported success");
            scope->throwError = true;
            run();
            require(warnings == originalWarnings + 2 && lastMessage.contains("offline transfer error"),
                    "transfer exception was not reported");
            require(remembered == 5 && scope->transfers == 5, "workflow count mismatch");
            scope->throwUnknown = true;
            run();
            require(warnings == originalWarnings + 3 && lastMessage.contains("Unknown capture error"),
                    "non-standard exception was not reported");
            require(scope->ranOnWorker, "transfer ran on application thread");
        }
        auto multiScope = std::make_shared<WaveformScope>();
        multiScope->failChannel = 3;
        CaptureContext multi;
        multi.oscilloscope = multiScope;
        multi.captureSession = std::make_shared<CaptureSession>();
        multi.selectSaveFile = [&](const QString&, const QString&, const QString&) {
            return dir.filePath("active.csv");
        };
        const int originalInfos = infos;
        AllCsvCaptureCommand(multi).execute();
        require(QThreadPool::globalInstance()->waitForDone(3000), "multi-channel capture did not finish");
        QCoreApplication::processEvents();
        require(multiScope->hostPaths == QStringList{dir.filePath("active_CH1.csv"), dir.filePath("active_CH3.csv")},
                "enabled channel filenames changed");
        require(infos == originalInfos + 1 && lastMessage.contains("Saved channels: 1")
                    && lastMessage.contains("Failed channels: CH3") && !multi.captureSession->isBusy(),
                "partial capture result or lease release failed");
        std::cout << "PASS: CSV/WFM metadata, channel selection, transfer arguments and async errors\n";
        return 0;
    } catch (const std::exception& e) {
        QThreadPool::globalInstance()->waitForDone();
        std::cerr << e.what() << '\n';
        return 1;
    }
}
