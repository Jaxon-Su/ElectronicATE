#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include "ixmlserializable.h"

class Page1ViewModel;
class Page2ViewModel;
class Page3ViewModel;
class Page4ViewModel;
class Page5ViewModel;

class AppService : public QObject {
    Q_OBJECT

public:
    static AppService& instance();

    void setupPageConnections(Page1ViewModel* vm1,
                              Page2ViewModel* vm2,
                              Page3ViewModel* vm3,
                              Page5ViewModel* vm5);

    // ── XML I/O（接受 IXmlSerializable 介面，不需知道具體 Page 型別）─────
    void saveAllToXml(const QString& fileName,
                      const QList<IXmlSerializable*>& pages);

    void loadAllFromXml(const QString& fileName,
                        const QList<IXmlSerializable*>& pages);

    void registerMetaTypes();

private:
    AppService(QObject* parent = nullptr);
    ~AppService() = default;
    AppService(const AppService&) = delete;
    AppService& operator=(const AppService&) = delete;

    void connectPage1ToPage2(Page1ViewModel* vm1, Page2ViewModel* vm2);
    void connectPage1ToPage3(Page1ViewModel* vm1, Page3ViewModel* vm3);
    void connectPage2ToPage3(Page2ViewModel* vm2, Page3ViewModel* vm3);
    void connectPage1ToPage5(Page1ViewModel* vm1, Page5ViewModel* vm5);
    void connectPage2ToPage5(Page2ViewModel* vm2, Page5ViewModel* vm5);
};
