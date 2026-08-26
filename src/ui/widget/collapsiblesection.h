#pragma once
#include <QWidget>
#include <QToolButton>
#include <QFrame>
#include <QVBoxLayout>
#include <QPropertyAnimation>
#include <QTimer>


class CollapsibleSection : public QWidget
{
    Q_OBJECT
public:
    explicit CollapsibleSection(const QString &title, QWidget *parent = nullptr);

    // 設定內容區域的 Layout，把你的 widgets 加進這裡
    QVBoxLayout* contentLayout() const { return m_contentLayout; }

    void setExpanded(bool expanded);
    bool isExpanded() const;
    void setExpandedDelayed(bool expanded);

private slots:
    void onToggleClicked();

private:
    QToolButton  *m_toggleButton;
    QFrame       *m_contentArea;
    QVBoxLayout  *m_contentLayout;
    QPropertyAnimation *m_animation;
    bool          m_expanded = false;
};
