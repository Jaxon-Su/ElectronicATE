#include "tablewidget.h"
#include "page1.h"
#include "page2.h"
#include "page3.h"
#include "page4.h"
#include "page5.h"
#include <QVBoxLayout>
#include <QTabBar>
#include <QSize>
#include <QPalette>
#include <QColor>

TableWidget::TableWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUI();
}

void TableWidget::setupUI()
{
    m_tabWidget = new QTabWidget(this);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tabWidget);

    // 調整圖片大小
    m_tabWidget->tabBar()->setIconSize(QSize(40, 40));

    // 設定背景色
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(240, 240, 240));
    setPalette(pal);
}

template<typename T>
void TableWidget::setPageInternal(T*& currentPage, T* newPage,
                                  const QString& iconPath, const QString& title)
{
    // 移除舊頁面
    if (currentPage) {
        int index = m_tabWidget->indexOf(currentPage);
        if (index >= 0) {
            m_tabWidget->removeTab(index);
        }
    }

    // 設置新頁面
    currentPage = newPage;
    if (currentPage) {
        currentPage->setParent(this);
        m_tabWidget->addTab(currentPage, QIcon(iconPath), title);
    }
}

void TableWidget::setPage1(Page1* page)
{
    setPageInternal(m_page1, page, ":/images/connection.png", "Instruments");
}

void TableWidget::setPage2(Page2* page)
{
    setPageInternal(m_page2, page, ":/images/condition.png", "Conditions");
}

void TableWidget::setPage3(Page3* page)
{
    setPageInternal(m_page3, page, ":/images/control.png", "Control");
}

void TableWidget::setPage4(Page4* page)
{
    setPageInternal(m_page4, page, ":/images/tasks.png", "Commands");
}

void TableWidget::setPage5(Page5* page)
{
    setPageInternal(m_page5, page, ":/images/queue.png", "Queue Tasks");
}

// ── setOtherTabsLocked ────────────────────────────────
// locked=true  → 停用所有 tab，只保留 keepIndex 可點擊
// locked=false → 全部恢復可點擊
void TableWidget::setOtherTabsLocked(bool locked, int keepIndex)
{
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        const bool enable = !locked || (i == keepIndex);
        m_tabWidget->setTabEnabled(i, enable);
    }

    // 鎖定時強制切換到 keepIndex，確保使用者看到執行頁面
    if (locked)
        m_tabWidget->setCurrentIndex(keepIndex);
}
