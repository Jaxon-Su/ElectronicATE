#pragma once

#include <QObject>
#include <QString>

class Page1Model;
class Page2Model;
class Page3Model;
class Page4Model;
class Page5Model;

class MainWindowModel : public QObject {
    Q_OBJECT

public:
    explicit MainWindowModel(QObject* parent = nullptr);
    ~MainWindowModel();

    // 檔案路徑管理
    const QString& lastSavePath() const { return m_lastSavePath; }
    void setLastSavePath(const QString& path);

    // 子模組存取
    Page1Model* page1Model() const { return m_page1Model; }
    Page2Model* page2Model() const { return m_page2Model; }
    Page3Model* page3Model() const { return m_page3Model; }
    Page4Model* page4Model() const { return m_page4Model; }
    Page5Model* page5Model() const { return m_page5Model; }

private:
    // 檔案路徑
    QString m_lastSavePath;

    // 子模組 - 使用 nullptr 初始化
    Page1Model* m_page1Model = nullptr;
    Page2Model* m_page2Model = nullptr;
    Page3Model* m_page3Model = nullptr;
    Page4Model* m_page4Model = nullptr;
    Page5Model* m_page5Model = nullptr;

    void initializeSubModels();
};
