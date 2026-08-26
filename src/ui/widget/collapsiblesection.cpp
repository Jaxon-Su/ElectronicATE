#include "collapsiblesection.h"

CollapsibleSection::CollapsibleSection(const QString &title, QWidget *parent)
    : QWidget(parent)
{
    // --- 標題按鈕 ---
    m_toggleButton = new QToolButton(this);
    m_toggleButton->setText(title);
    m_toggleButton->setCheckable(true);
    m_toggleButton->setChecked(false);
    m_toggleButton->setArrowType(Qt::RightArrow);
    m_toggleButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon); //箭頭與文字的排列方式
    m_toggleButton->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_toggleButton->setStyleSheet(R"(
        QToolButton {
            background-color: #3c3f41;
            color: white;
            border: none;
            padding: 6px;
            font-weight: bold;
            text-align: left;
        }
        QToolButton:hover { background-color: #4c5052; }
        QToolButton:checked { background-color: #2d5a8e; }
    )");

    // --- 內容區域 ---
    m_contentArea = new QFrame(this);
    m_contentArea->setFrameShape(QFrame::NoFrame);
    m_contentArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_contentArea->setMaximumHeight(0);
    m_contentArea->setMinimumHeight(0);
    m_contentArea->setStyleSheet("background-color: #2b2b2b;");

    m_contentLayout = new QVBoxLayout(m_contentArea);
    m_contentLayout->setContentsMargins(10, 5, 10, 5);
    m_contentLayout->setSpacing(6);

    // --- 動畫 ---
    m_animation = new QPropertyAnimation(m_contentArea, "maximumHeight", this);
    m_animation->setDuration(200);
    m_animation->setEasingCurve(QEasingCurve::InOutQuad);

    // --- 主 Layout ---
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_toggleButton);
    mainLayout->addWidget(m_contentArea);

    connect(m_toggleButton, &QToolButton::clicked, this, &CollapsibleSection::onToggleClicked);
}

void CollapsibleSection::onToggleClicked()
{
    setExpanded(!m_expanded);
}

void CollapsibleSection::setExpanded(bool expanded)
{
    m_expanded = expanded;
    m_toggleButton->setChecked(expanded);
    m_toggleButton->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);

    // 計算內容實際高度
    int contentHeight = m_contentLayout->sizeHint().height()
                        + m_contentLayout->contentsMargins().top()
                        + m_contentLayout->contentsMargins().bottom();

    m_animation->stop();
    m_animation->setStartValue(m_contentArea->maximumHeight());
    m_animation->setEndValue(expanded ? contentHeight : 0);
    m_animation->start();
}

bool CollapsibleSection::isExpanded() const
{
    return m_expanded;
}

void CollapsibleSection::setExpandedDelayed(bool expanded)
{
    QTimer::singleShot(0, this, [this, expanded]() {
        setExpanded(expanded);
    });
}
