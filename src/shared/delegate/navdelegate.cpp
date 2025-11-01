#include "NavDelegate.h"

#include <QWidget>
#include <QStyleOptionViewItem>
#include <QModelIndex>
NavDelegate::NavDelegate(QObject* parent, QObject* filterTarget)
    : QStyledItemDelegate(parent)
    , m_filterTarget(filterTarget)
{}

QWidget* NavDelegate::createEditor(QWidget *parent,
                                   const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const
{
    QWidget* ed = QStyledItemDelegate::createEditor(parent, option, index);
    if (ed && m_filterTarget)
        ed->installEventFilter(m_filterTarget);
    return ed;
}

void NavDelegate::initStyleOption(QStyleOptionViewItem *opt,
                                  const QModelIndex &idx) const
{
    QStyledItemDelegate::initStyleOption(opt, idx);
    opt->displayAlignment = Qt::AlignCenter;
}
