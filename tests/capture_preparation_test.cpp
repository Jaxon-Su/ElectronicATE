#include "pngcapturecommand.h"
#include "csvcapturecommand.h"
#include "allcsvcapturecommand.h"
#include "wfmcapturecommand.h"
#include "oscilloscope.h"
#include "messageservice.h"
#include <QCoreApplication>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

class PreparationScope : public Oscilloscope {
public:
    bool failQuery = true;
    int captures = 0;
    QString model() const override { return "OfflineScope"; }
    QString vendor() const override { return "Test"; }
    bool isConnected() const override { return true; }
    QString getTriggerSource() override {
        if (failQuery) throw std::runtime_error("query failure");
        return "CH1";
    }
    int getTotalChannel() override {
        if (failQuery) throw std::runtime_error("query failure");
        return 1;
    }
    bool isChannelEnabled(int) override { return true; }
    QByteArray captureScreenshot(const QString&, const QString&) override { ++captures; return {}; }
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        int warnings = 0;
        QString message;
        QObject::connect(&MessageService::instance(), &MessageService::warningRequested, &app,
            [&](const QString&, const QString& text) { ++warnings; message = text; });
        auto scope = std::make_shared<PreparationScope>();
        CaptureContext context;
        context.oscilloscope = scope;
        context.captureSession = std::make_shared<CaptureSession>();
        CsvCaptureCommand(context).execute();
        WfmCaptureCommand(context).execute();
        AllCsvCaptureCommand(context).execute();
        require(warnings == 3 && message.contains("query failure") && !context.captureSession->isBusy(),
                "query exception escaped or left capture busy");
        scope->failQuery = false;
        for (int command = 0; command < 4; ++command) {
            context.selectSaveFile = [](const QString&, const QString&, const QString&) -> QString { throw 42; };
            auto run = [&] {
                switch (command) {
                case 0: PngCaptureCommand(context).execute(); break;
                case 1: CsvCaptureCommand(context).execute(); break;
                case 2: WfmCaptureCommand(context).execute(); break;
                case 3: AllCsvCaptureCommand(context).execute(); break;
                }
            };
            int before = warnings;
            run();
            require(warnings == before + 1 && message.contains("Unknown capture error") && !context.captureSession->isBusy(),
                    "selector exception escaped or leaked lease");
            context.selectSaveFile = [](const QString&, const QString&, const QString&) { return QString("unused.png"); };
            context.onSaveDirChanged = [](const QString&) { throw std::runtime_error("preference failure"); };
            before = warnings;
            run();
            require(warnings == before + 1 && message.contains("preference failure") && !context.captureSession->isBusy(),
                    "preference exception escaped or leaked lease");
            context.selectSaveFile = {};
            before = warnings;
            run();
            require(warnings == before && !context.captureSession->isBusy(), "retry could not cancel cleanly");
        }
        require(scope->captures == 0, "failed preparation submitted capture");
        std::cout << "PASS: preparation errors release leases and allow retry for every capture command\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
