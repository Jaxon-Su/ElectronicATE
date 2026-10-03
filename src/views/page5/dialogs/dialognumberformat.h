#pragma once
#include "oscilloscopesettingrules.h"
#include <QDoubleSpinBox>
#include <QLocale>

namespace Page5NumberFormat {
inline constexpr int decimals = 4;

class DecimalSpinBox final : public QDoubleSpinBox {
  public:
    explicit DecimalSpinBox(QWidget *parent = nullptr) : QDoubleSpinBox(parent)
    { setDecimals(Page5NumberFormat::decimals); }

    QString textFromValue(double value) const override
    {
        QString text = QDoubleSpinBox::textFromValue(value);
        const QString point = locale().decimalPoint();
        const QString zero = locale().zeroDigit();
        if (text.contains(point)) {
            while (text.endsWith(zero)) text.chop(zero.size());
            if (text.endsWith(point)) text.chop(point.size());
        }
        return text;
    }
};

// Format in probe units; rounding a small SI value directly could turn it into zero.
inline QString channelScale(const QString &text)
{
    const auto value = OscilloscopeSettingRules::quantity(text, "channel");
    if (!value)
        return text.trimmed(); // Leave invalid input visible for existing validation.
    const auto trimmed = text.trimmed();
    QString unit = trimmed.endsWith('A') ? "A" : trimmed.endsWith('V') ? "V" : "";
    double displayed = *value;
    if (!unit.isEmpty() && displayed < 1) {
        const bool micro = displayed < 0.001;
        displayed *= micro ? 1e6 : 1e3;
        unit.prepend(micro ? "u" : "m");
    }
    QString number = QString::number(displayed, displayed < std::pow(10.0, -decimals) ? 'e' : 'f', decimals);
    const int exponentIndex = number.indexOf('e');
    const QString exponent = exponentIndex < 0 ? QString{} : number.mid(exponentIndex);
    if (exponentIndex >= 0) number.truncate(exponentIndex);
    while (number.endsWith('0')) number.chop(1);
    if (number.endsWith('.')) number.chop(1);
    return number + exponent + unit;
}
}
