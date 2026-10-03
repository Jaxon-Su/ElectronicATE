#include "mainwindow.h"
#include "applicationinfo.h"
#include <QApplication>
#include "src/hardware/communication/commscanner.h"
#include <cstring>

int main(int argc, char *argv[])
{
    if (argc == 2 && std::strcmp(argv[1], "--scan-comm-helper") == 0) {
        QCoreApplication app(argc, argv);
        return runCommScanner();
    }
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QString::fromUtf8(ApplicationInfo::Name));
    QCoreApplication::setApplicationVersion(QString::fromUtf8(ApplicationInfo::Version));

    MainWindow *window = new MainWindow();
    window->show();

    return app.exec();
}
