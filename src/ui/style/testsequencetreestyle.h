#pragma once
#include <QProxyStyle>

class TestSequenceTreeStyle : public QProxyStyle {
  public:
    explicit TestSequenceTreeStyle(QStyle* baseStyle = nullptr);
    ~TestSequenceTreeStyle() override = default;

    void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter,
                       const QWidget* widget = nullptr) const override;
};
