#pragma once
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <QScrollArea>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>

class SidePanel : public QWidget
{
    Q_OBJECT

public:
    explicit SidePanel(Qt::Edge side,
                       int     defaultWidth = 240,
                       QWidget *parent      = nullptr);

    QVBoxLayout* contentLayout() const { return m_contentLayout; }
    bool         isExpanded()    const { return m_expanded; }
    int          expandedWidth() const { return m_lastWidth; }

    void setExpanded(bool expanded, bool animate = true);

signals:
    void expandedChanged(bool expanded);

private:
    void onToggleClicked();
    void updateArrow();

public:
    static constexpr int BTN_W     = 18;   // 箭頭按鈕固定寬度（px），page5.cpp 可用
    static constexpr int ANIM_MS   = 220;  // 動畫時長（ms）

private:
    Qt::Edge            m_side;
    int                 m_lastWidth;        // 記住展開寬度（含拖拉結果）
    bool                m_expanded      = true;

    QToolButton        *m_toggleButton  = nullptr;
    QScrollArea        *m_scrollArea    = nullptr;
    QVBoxLayout        *m_contentLayout = nullptr;

    // 收折用：縮小 this 的 maximumWidth（Splitter 自動跟著縮）
    QPropertyAnimation *m_collapseAnim  = nullptr;
    // 展開用：scrollArea 從 0 滑出
    QPropertyAnimation *m_expandAnim    = nullptr;
};
