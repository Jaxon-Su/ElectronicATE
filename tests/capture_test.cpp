#include "pngcapturecommand.h"
#include "csvcapturecommand.h"
#include "allcsvcapturecommand.h"
#include "wfmcapturecommand.h"
#include "oscilloscope.h"
#include "messageservice.h"
#include "capturelease.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QThreadPool>
#include <QTimer>
#include <iostream>
#include <atomic>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

class FakeScope : public Oscilloscope {
public:
    std::atomic<int> captures{0};
    std::atomic<int> channelQueries{0};
    bool emptyImage = false;
    QString model() const override { return "OfflineScope"; }
    QString vendor() const override { return "Test"; }
    bool isConnected() const override { return true; }
    QString getTriggerSource() override { ++channelQueries; return "CH1"; }
    int getTotalChannel() override { ++channelQueries; return 2; }
    bool isChannelEnabled(int) override { ++channelQueries; return true; }
    QByteArray captureScreenshot(const QString& format, const QString&) override
    {
        ++captures;
        return !emptyImage && format == "PNG" ? QByteArray("offline-image") : QByteArray{};
    }
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        int warnings = 0;
        QObject::connect(&MessageService::instance(), &MessageService::warningRequested,
                         &app, [&](const QString&, const QString&) { ++warnings; });
        CaptureContext empty;
        PngCaptureCommand(empty).execute();
        CsvCaptureCommand(empty).execute();
        AllCsvCaptureCommand(empty).execute();
        WfmCaptureCommand(empty).execute();
        require(warnings == 4, "missing scopes should report without dialogs");

        auto scope = std::make_shared<FakeScope>();
        CaptureContext context;
        context.oscilloscope = scope;
        context.captureSession = std::make_shared<CaptureSession>();
        require(!CaptureLease::tryAcquire({}), "null capture flag should not acquire");
        {
            auto lease = CaptureLease::tryAcquire(context.captureSession);
            require(lease && context.captureSession->isBusy(), "lease acquisition failed");
            auto transferred = lease;
            lease.reset();
            require(context.captureSession->isBusy() && !CaptureLease::tryAcquire(context.captureSession),
                    "shared lease failed to preserve exclusivity");
        }
        require(!context.captureSession->isBusy(), "last lease owner did not release");
        PngCaptureCommand(context).execute();
        require(scope->captures == 0 && !context.captureSession->isBusy(),
                "absent selector should cancel without capture");
        int selected = 0;
        int remembered = 0;
        context.selectSaveFile = [&](const QString&, const QString&, const QString&) {
            ++selected;
            return QString{};
        };
        context.onSaveDirChanged = [&](const QString&) { ++remembered; };
        PngCaptureCommand(context).execute();
        require(selected == 1 && remembered == 0 && scope->captures == 0,
                "cancel should not update preference or acquire capture");

        QTemporaryDir dir;
        require(dir.isValid(), "temporary directory failed");
        const QString path = dir.filePath("screenshot.png");
        context.selectSaveFile = [&](const QString& title, const QString& suggested, const QString& filter) {
            require(title.contains("Screenshot") && suggested.contains("OfflineScope") && filter.contains("PNG"),
                    "selector metadata was not preserved");
            return path;
        };
        auto busyLease = CaptureLease::tryAcquire(context.captureSession);
        PngCaptureCommand(context).execute();
        CsvCaptureCommand(context).execute();
        AllCsvCaptureCommand(context).execute();
        WfmCaptureCommand(context).execute();
        require(warnings == 8 && scope->captures == 0 && scope->channelQueries == 0
                    && remembered == 0 && context.captureSession->isBusy(),
                "busy capture should not run or release another operation");
        busyLease.reset();

        QEventLoop loop;
        bool completed = false;
        QObject::connect(&MessageService::instance(), &MessageService::infoRequested,
                         &loop, [&](const QString& title, const QString& message) {
            completed = title == "Capture Complete" && message.contains(path);
            loop.quit();
        });
        QTimer::singleShot(3000, &loop, &QEventLoop::quit);
        PngCaptureCommand(context).execute();
        loop.exec();
        QThreadPool::globalInstance()->waitForDone();
        QFile file(path);
        require(file.open(QIODevice::ReadOnly) && file.readAll() == "offline-image", "capture data missing");
        require(completed && scope->captures == 1 && !context.captureSession->isBusy(),
                "completion notification or capture guard failed");
        file.close();
        const int originalWarnings = warnings;
        completed = false;
        scope->emptyImage = true;
        PngCaptureCommand(context).execute();
        require(QThreadPool::globalInstance()->waitForDone(3000), "empty image capture did not finish");
        QCoreApplication::processEvents();
        require(warnings == originalWarnings + 1 && !completed && !context.captureSession->isBusy(),
                "empty image reported success or leaked lease");
        require(file.open(QIODevice::ReadOnly) && file.readAll() == "offline-image", "failed capture changed existing file");
        file.close();
        scope->emptyImage = false;
        context.selectSaveFile = [&](const QString&, const QString&, const QString&) {
            return dir.filePath("missing/screenshot.png");
        };
        PngCaptureCommand(context).execute();
        require(QThreadPool::globalInstance()->waitForDone(3000), "failed save did not finish");
        QCoreApplication::processEvents();
        require(warnings == originalWarnings + 2 && !completed && !context.captureSession->isBusy(),
                "failed save reported success or leaked lease");
        std::cout << "PASS: capture errors, cancel, busy and async success without widgets or real hardware\n";
        return 0;
    } catch (const std::exception& e) {
        QThreadPool::globalInstance()->waitForDone();
        std::cerr << e.what() << '\n';
        return 1;
    }
}
