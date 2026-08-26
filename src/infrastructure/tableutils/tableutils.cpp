#include "tableutils.h"
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QFont>
#include <QColor>
#include <QHeaderView>

namespace TableUtils
{

// ── 捲軸共用樣式 ─────────────────────────────────────
static const QString SCROLLBAR_STYLE = R"(
    QAbstractScrollArea {
        background-color: #ffffff;
    }
    QAbstractScrollArea::corner {
        background-color: #f0f4fa;
        border: 1px solid #c5cfe0;
    }
    QScrollBar:horizontal {
        background: #f0f4fa;
        height: 10px;
        margin: 0px;
        border-radius: 5px;
    }
    QScrollBar::handle:horizontal {
        background: #b0bcd4;
        min-width: 20px;
        border-radius: 5px;
    }
    QScrollBar::handle:horizontal:hover {
        background: #7a9cc8;
    }
    QScrollBar::add-line:horizontal,
    QScrollBar::sub-line:horizontal {
        width: 0px;
    }
    QScrollBar:vertical {
        background: #f0f4fa;
        width: 10px;
        margin: 0px;
        border-radius: 5px;
    }
    QScrollBar::handle:vertical {
        background: #b0bcd4;
        min-height: 20px;
        border-radius: 5px;
    }
    QScrollBar::handle:vertical:hover {
        background: #7a9cc8;
    }
    QScrollBar::add-line:vertical,
    QScrollBar::sub-line:vertical {
        height: 0px;
    }
)";

QTableWidgetItem* makeCenteredItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

QTableWidgetItem* makeMetaItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);
    item->setBackground(QColor("#e8ecf4"));
    item->setForeground(QColor("#2c3e60"));
    QFont f = item->font();
    f.setBold(true);
    item->setFont(f);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

QWidget* makeCenteredCheckbox(bool checked)
{
    auto* w = new QWidget;
    w->setStyleSheet("background: transparent;");
    auto* layout = new QHBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setAlignment(Qt::AlignCenter);

    auto* btn = new QPushButton(checked ? "✓" : "");
    btn->setCheckable(true);
    btn->setChecked(checked);
    btn->setFixedSize(18, 18);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setStyleSheet(R"(
        QPushButton {
            border: 2px solid #aab4c8;
            border-radius: 3px;
            background: #ffffff;
            font-size: 13px;
            font-weight: bold;
            color: #000000;
            padding: 0px;
        }
        QPushButton:checked {
            border: 2px solid #2a6dd9;
            background: #ffffff;
        }
        QPushButton:hover {
            border: 2px solid #2a6dd9;
            background: #f0f5ff;
        }
    )");
    QObject::connect(btn, &QPushButton::toggled, btn, [btn](bool on) {
        btn->setText(on ? "✓" : "");
    });

    layout->addWidget(btn);
    return w;
}

void fitTableHeight(QTableWidget* table, int maxH)
{
    const int hdrH = table->horizontalHeader()->height();
    int rowTotal = 0;
    for (int r = 0; r < table->rowCount(); ++r)
        rowTotal += table->rowHeight(r);
    const int scrollH = (table->horizontalScrollBar() &&
                         table->horizontalScrollBar()->isVisible()) ? 18 : 0;
    const int h = qMin(hdrH + rowTotal + scrollH + 6, maxH);
    table->setMaximumHeight(qMax(h, 44));
}

void applyTableStyle(QTableWidget* table, const QString& baseStyle)
{
    table->setStyleSheet(baseStyle + SCROLLBAR_STYLE);
    table->setAlternatingRowColors(true);
    table->setShowGrid(true);
}

} // namespace TableUtils
