#pragma once

#include <QObject>
#include <QString>

class Page1Model;
class Page2Model;
class Page3Model;
class Page4Model;

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

private:
    QString m_lastSavePath;

    // 子模組
    Page1Model* m_page1Model;
    Page2Model* m_page2Model;
    Page3Model* m_page3Model;
    Page4Model* m_page4Model;

    void initializeSubModels();
};
