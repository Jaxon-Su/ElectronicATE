// NavDelegate.h
#pragma once

#include <QStyledItemDelegate>       // 繼承需要完整定義

class QWidget;                        // 前置宣告即可
class QStyleOptionViewItem;
class QModelIndex;

/// NavDelegate 支援鍵盤上下左右導覽，並將 cell 內容置中
class NavDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    /// @param parent      Delegate 所屬的 QObject（通常傳 this）
    /// @param filterTarget eventFilter 的目標（通常是你的 ViewModel）
    explicit NavDelegate(QObject *parent = nullptr,
                         QObject *filterTarget = nullptr);

    /// 建立編輯器時安裝 eventFilter
    QWidget* createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

    /// 委派顯示時置中對齊
    void initStyleOption(QStyleOptionViewItem *opt,
                         const QModelIndex &idx) const override;

private:
    QObject *m_filterTarget;
};
