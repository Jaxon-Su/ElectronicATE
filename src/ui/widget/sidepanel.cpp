#include "sidepanel.h"
#include "page5style.h"

SidePanel::SidePanel(Qt::Edge side, int defaultWidth, QWidget *parent)
    : QWidget(parent)
    , m_side(side)
    , m_lastWidth(defaultWidth)
{
    // ── 箭頭按鈕（永遠可見）──
    m_toggleButton = new QToolButton(this);
    m_toggleButton->setFixedWidth(BTN_W);
    m_toggleButton->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_toggleButton->setStyleSheet(Page5Style::SIDE_TOGGLE_BTN);
    connect(m_toggleButton, &QToolButton::clicked,
            this, &SidePanel::onToggleClicked);

    // ── 內容區 ──
    auto *contentWidget = new QWidget;
    contentWidget->setStyleSheet("background-color: #f5f5f5;");
    m_contentLayout = new QVBoxLayout(contentWidget);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(2);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidget(contentWidget);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setStyleSheet("QScrollArea { border: none; background: transparent; }");
    m_scrollArea->setMinimumWidth(0); // 不固定寬度，由 SidePanel 自身寬度決定

    // ══ 收折動畫：對 this->maximumWidth 做動畫 ══
    // 好處：QSplitter 會監聽子 widget 的 maximumWidth 變化，
    //       自動把釋放出的空間分配給中間面板，不會留下灰底。
    // QPropertyAnimation(QObject *target, const QByteArray &propertyName, QObject *parent = nullptr);
    m_collapseAnim = new QPropertyAnimation(this, "maximumWidth", this);
    m_collapseAnim->setDuration(ANIM_MS); //設定動畫跑完一次需要多久
    //動畫從起點到終點，數值變化的速度曲線，也就是「加速/減速的方式」。
    //所以整體效果是：開始慢(In) → 中間快(Out) → 結束慢(Quad)，像現實中物體的自然運動感。
    m_collapseAnim->setEasingCurve(QEasingCurve::InOutQuad);
    // 收折完成：隱藏 scrollArea，讓面板只顯示箭頭按鈕條
    connect(m_collapseAnim, &QPropertyAnimation::finished, this, [this]() {
        m_scrollArea->hide();
    });

    // ══ 展開動畫：對 scrollArea->maximumWidth 做動畫 ══
    // 展開前先用 setMinimumWidth 強制 Splitter 騰出空間，
    // 再讓 scrollArea 從 0 滑到目標寬度（視覺上內容滑出）。
    m_expandAnim = new QPropertyAnimation(m_scrollArea, "maximumWidth", this);
    m_expandAnim->setDuration(ANIM_MS);
    m_expandAnim->setEasingCurve(QEasingCurve::InOutQuad);
    // minimumWidth 同步，避免 layout 壓縮 scrollArea
    connect(m_expandAnim, &QPropertyAnimation::valueChanged,
            this, [this](const QVariant &val) {
                m_scrollArea->setMinimumWidth(val.toInt());
            });
    // 展開完成：解除 SidePanel 的寬度限制，讓 Splitter 可以自由拖拉
    connect(m_expandAnim, &QPropertyAnimation::finished, this, [this]() {
        setMinimumWidth(BTN_W);
        setMaximumWidth(QWIDGETSIZE_MAX);
        m_scrollArea->setMinimumWidth(0);
        m_scrollArea->setMaximumWidth(QWIDGETSIZE_MAX);
    });

    // ── 排版 ──
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    //Qt::Edge 是 Qt 內建的列舉（enum），用來表示哪一個邊（上/下/左/右）。
    if (m_side == Qt::LeftEdge) {
        layout->addWidget(m_scrollArea);
        layout->addWidget(m_toggleButton);
    } else {
        layout->addWidget(m_toggleButton);
        layout->addWidget(m_scrollArea);
    }

    setMinimumWidth(BTN_W);
    updateArrow();
}

void SidePanel::onToggleClicked()
{
    // 收折前記住目前寬度（可能被 Splitter 拖過）
    if (m_expanded && width() > BTN_W)
        m_lastWidth = width();

    setExpanded(!m_expanded, true);
}

void SidePanel::setExpanded(bool expanded, bool animate)
{
    m_expanded = expanded;
    updateArrow();
    emit expandedChanged(expanded);

    if (animate) {
        //stop 防止動畫衝突 假設用戶快速連點按鈕
        m_collapseAnim->stop();
        m_expandAnim->stop();

        if (!expanded) {
            // ── 收折：動畫 this->maximumWidth → Splitter 自動縮 ──
            m_collapseAnim->setStartValue(width());
            m_collapseAnim->setEndValue(BTN_W);
            m_collapseAnim->start();
        } else {
            // ── 展開：先強制 Splitter 騰出空間，再滑出 scrollArea ──
            m_scrollArea->show();
            m_scrollArea->setMaximumWidth(0);
            m_scrollArea->setMinimumWidth(0);

            // 鎖定 SidePanel 寬度 = lastWidth，Splitter 必須給足空間
            setMinimumWidth(m_lastWidth);
            setMaximumWidth(m_lastWidth);

            // scrollArea 從 0 滑到 lastWidth
            m_expandAnim->setStartValue(0);
            m_expandAnim->setEndValue(m_lastWidth);
            m_expandAnim->start();
        }
    } else {
        // 無動畫版本
        m_collapseAnim->stop();
        m_expandAnim->stop();
        if (!expanded) {
            setMaximumWidth(BTN_W);
            m_scrollArea->hide();
        } else {
            m_scrollArea->show();
            m_scrollArea->setMinimumWidth(0); // 不固定寬度，由 SidePanel 自身寬度決定
            setMinimumWidth(BTN_W);
            setMaximumWidth(QWIDGETSIZE_MAX);
        }
    }
}

void SidePanel::updateArrow()
{
    const bool isLeft = (m_side == Qt::LeftEdge);
    // 左側展開：◀（可收）  左側收折：▶（可展）
    // 右側展開：▶（可收）  右側收折：◀（可展）
    m_toggleButton->setArrowType(
        m_expanded ? (isLeft ? Qt::LeftArrow  : Qt::RightArrow)
                   : (isLeft ? Qt::RightArrow : Qt::LeftArrow)
        );
}
