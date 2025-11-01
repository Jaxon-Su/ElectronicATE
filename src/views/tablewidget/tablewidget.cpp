#include "tablewidget.h"
#include <QVBoxLayout>
#include <QTabBar>
#include <QSize>
#include <QPalette>
#include <QColor>

TableWidget::TableWidget(QWidget *parent)
    : QWidget(parent)
    , m_tabWidget(nullptr)
    , m_page1(nullptr)
    , m_page2(nullptr)
    , m_page3(nullptr)
    , m_page4(nullptr)
{
    setupUI();
    // setupTabWidget();
}

void TableWidget::setupUI()
{
    m_tabWidget = new QTabWidget(this);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_tabWidget);
    setLayout(layout);

    // 調整圖片大小
    m_tabWidget->tabBar()->setIconSize(QSize(40, 40));

    // 啟用自動填充背景
    this->setAutoFillBackground(true);

    // 設定背景色透過 QPalette (最穩定的方法)
    QPalette pal = this->palette();
    pal.setColor(QPalette::Window, QColor(240, 240, 240)); // lightblue 顏色
    this->setPalette(pal);

}

// void TableWidget::setupTabWidget()
// {
//     // 初始時只建立 TabWidget，頁面會由 AppController 設定
// }

void TableWidget::setPage1(Page1* page)
{
    if (m_page1) {
        int index = m_tabWidget->indexOf(m_page1);
        if (index >= 0) {
            m_tabWidget->removeTab(index);
        }
    }

    m_page1 = page;
    if (m_page1) {
        m_page1->setParent(this);
        m_tabWidget->addTab(m_page1, QIcon(":/images/connection.png"), "Instruments");
    }
}

void TableWidget::setPage2(Page2* page)
{
    if (m_page2) {
        int index = m_tabWidget->indexOf(m_page2);
        if (index >= 0) {
            m_tabWidget->removeTab(index);
        }
    }

    m_page2 = page;
    if (m_page2) {
        m_page2->setParent(this);
        m_tabWidget->addTab(m_page2, QIcon(":/images/condition.png"), "Conditions");
    }
}

void TableWidget::setPage3(Page3* page)
{
    if (m_page3) {
        int index = m_tabWidget->indexOf(m_page3);
        if (index >= 0) {
            m_tabWidget->removeTab(index);
        }
    }

    m_page3 = page;
    if (m_page3) {
        m_page3->setParent(this);
        m_tabWidget->addTab(m_page3, QIcon(":/images/control.png"), "Control");
    }
}

void TableWidget::setPage4(Page4* page)
{
    if (m_page4) {
        int index = m_tabWidget->indexOf(m_page4);
        if (index >= 0) {
            m_tabWidget->removeTab(index);
        }
    }

    m_page4 = page;
    if (m_page4) {
        m_page4->setParent(this);
        m_tabWidget->addTab(m_page4, QIcon(":/images/tasks.png"), "Procedures");
    }
}
