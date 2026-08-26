#include "page5treestyle.h"
#include <QPainter>
#include <QStyleOption>

// ── 可調整參數 ─────────────────────────────────────────
namespace {
const QColor LINE_COLOR  = QColor("#AAAAAA"); // 連接線顏色
const QColor BOX_COLOR   = QColor("#666666"); // 展開/摺疊方框顏色
const Qt::PenStyle LINE_STYLE = Qt::DotLine;  // Qt::DotLine / Qt::SolidLine
const int    BOX_SIZE    = 9;                 // 展開/摺疊方框邊長（px）
const int    LINE_WIDTH  = 1;
}

// ── Constructor ───────────────────────────────────────
Page5TreeStyle::Page5TreeStyle(QStyle* baseStyle)
    : QProxyStyle(baseStyle)
{}

// ── drawPrimitive ─────────────────────────────────────
void Page5TreeStyle::drawPrimitive(PrimitiveElement    element,
                                   const QStyleOption* option,
                                   QPainter*           painter,
                                   const QWidget*      widget) const
{
    if (element != PE_IndicatorBranch) {
        QProxyStyle::drawPrimitive(element, option, painter, widget);
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);

    const QRect& r   = option->rect;
    const int    cx  = r.center().x();
    const int    cy  = r.center().y();

    const bool hasItem     = option->state & State_Item;     // 此格有同層 Item 相鄰
    const bool hasSibling  = option->state & State_Sibling;  // 下方還有兄弟節點
    const bool hasChildren = option->state & State_Children; // 有子節點
    const bool isOpen      = option->state & State_Open;     // 已展開

    // ── 虛線連接線 ────────────────────────────────────
    QPen linePen(LINE_COLOR, LINE_WIDTH, LINE_STYLE);
    linePen.setDashPattern({ 2, 2 }); // 可調虛線間距
    painter->setPen(linePen);

    // 垂直線
    if (hasSibling && hasItem)
        painter->drawLine(cx, r.top(), cx, r.bottom()); // T 型：完整穿越
    else if (hasSibling)
        painter->drawLine(cx, r.top(), cx, r.bottom()); // 純垂直穿越（無水平）
    else if (hasItem)
        painter->drawLine(cx, r.top(), cx, cy);          // L 型：只到中心

    // 水平線（連到 item 文字）
    if (hasItem) {
        // 若有展開方框則水平線從方框右邊開始
        const int hLineStart = hasChildren ? cx + BOX_SIZE / 2 + 1 : cx;
        painter->drawLine(hLineStart, cy, r.right(), cy);
    }

    // ── 展開 / 摺疊方框 [+] [-] ───────────────────────
    if (hasChildren) {
        const int  half = BOX_SIZE / 2;
        QRect box(cx - half, cy - half, BOX_SIZE, BOX_SIZE);

        // 白底（遮住後面的虛線）
        painter->setPen(Qt::NoPen);
        painter->setBrush(Qt::white);
        painter->drawRect(box);

        // 外框
        QPen boxPen(BOX_COLOR, LINE_WIDTH, Qt::SolidLine);
        painter->setPen(boxPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(box);

        // 橫線 ─
        painter->drawLine(box.left() + 2, cy, box.right() - 2, cy);

        // 縱線 |（摺疊時顯示，展開時隱藏）
        if (!isOpen)
            painter->drawLine(cx, box.top() + 2, cx, box.bottom() - 2);
    }

    painter->restore();
}
