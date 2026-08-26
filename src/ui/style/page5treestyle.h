#pragma once
#include <QProxyStyle>

class Page5TreeStyle : public QProxyStyle
{
public:
    explicit Page5TreeStyle(QStyle* baseStyle = nullptr);
    ~Page5TreeStyle() override = default;

    void drawPrimitive(PrimitiveElement    element,
                       const QStyleOption* option,
                       QPainter*           painter,
                       const QWidget*      widget = nullptr) const override;
};
