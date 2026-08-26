#pragma once

#include <QWidget>
#include <QTabWidget>

class Page1;
class Page2;
class Page3;
class Page4;
class Page5;

class TableWidget : public QWidget {
    Q_OBJECT

public:
    explicit TableWidget(QWidget *parent = nullptr);

    // 設置頁面
    void setPage1(Page1* page);
    void setPage2(Page2* page);
    void setPage3(Page3* page);
    void setPage4(Page4* page);
    void setPage5(Page5* page);

    // 取得 TabWidget（如需要外部存取）
    QTabWidget* tabWidget() const { return m_tabWidget; }

    // 執行中鎖定其他 tab，只保留 keepIndex 可點擊
    void setOtherTabsLocked(bool locked, int keepIndex);

private:
    QTabWidget* m_tabWidget = nullptr;

    // 頁面指標
    Page1* m_page1 = nullptr;
    Page2* m_page2 = nullptr;
    Page3* m_page3 = nullptr;
    Page4* m_page4 = nullptr;
    Page5* m_page5 = nullptr;

    void setupUI();

    template<typename T>
    void setPageInternal(T*& currentPage, T* newPage,
                         const QString& iconPath, const QString& title);
};
