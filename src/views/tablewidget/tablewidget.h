#pragma once
#include <QWidget>
#include <QTabWidget>
#include "page1.h"
#include "page2.h"
#include "page3.h"
#include "page4.h"

class TableWidget : public QWidget {
    Q_OBJECT

public:
    explicit TableWidget(QWidget *parent = nullptr);

    // 設置頁面
    void setPage1(Page1* page);
    void setPage2(Page2* page);
    void setPage3(Page3* page);
    void setPage4(Page4* page);

private:
    QTabWidget* m_tabWidget;
    Page1* m_page1;
    Page2* m_page2;
    Page3* m_page3;
    Page4* m_page4;

    void setupUI();
    // void setupTabWidget();
};

// 管理 QTabWidget 的頁簽
// 提供頁面設定介面（setPage1/2/3）
// 處理 UI 樣式（背景色、圖示大小）
