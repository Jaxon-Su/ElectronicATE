#pragma once

#include <QObject>
#include <QString>

class Page1;
class Page2;
class Page3;
class Page1ViewModel;
class Page2ViewModel;
class Page3ViewModel;

class AppService : public QObject {
    Q_OBJECT

public:
    static AppService& instance();

    // 頁面間連接管理
    void setupPageConnections(Page1ViewModel* vm1,
                              Page2ViewModel* vm2,
                              Page3ViewModel* vm3);

    // XML 序列化
    void saveAllToXml(const QString& fileName,
                      Page1* page1, Page2* page2, Page3* page3,
                      Page1ViewModel* vm1, Page2ViewModel* vm2, Page3ViewModel* vm3);

    void loadAllFromXml(const QString& fileName,
                        Page1ViewModel* vm1, Page2ViewModel* vm2, Page3ViewModel* vm3);

    // Meta Types 註冊
    void registerMetaTypes();

private:
    AppService(QObject* parent = nullptr);
    ~AppService() = default;

    // 防止複製
    AppService(const AppService&) = delete;
    AppService& operator=(const AppService&) = delete;

    // 連接方法
    void connectPage1ToPage2(Page1ViewModel* vm1, Page2ViewModel* vm2);
    void connectPage1ToPage3(Page1ViewModel* vm1, Page3ViewModel* vm3);
    void connectPage2ToPage3(Page2ViewModel* vm2, Page3ViewModel* vm3);
};
