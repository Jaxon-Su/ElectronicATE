#pragma once

#include <QString>

namespace Page5DialogStyle {
// Shared typography; individual dialogs retain their colors and layout.
inline QString withTypography(const char* style)
{
    return QString::fromUtf8(style) + QStringLiteral(R"(
        QWidget { font-family: "Segoe UI"; font-size: 12px; }
        QLabel#titleLbl { font-size: 15px; font-weight: bold; }
        QCheckBox#autoPeriod, QCheckBox#captureExtrema { color: #1a2a4a; spacing: 8px; }
        QCheckBox#autoPeriod::indicator, QCheckBox#captureExtrema::indicator { width: 16px; height: 16px; }
        QCheckBox#autoPeriod::indicator:unchecked, QCheckBox#captureExtrema::indicator:unchecked { image: url(:/images/checkbox-unchecked.svg); }
        QCheckBox#autoPeriod::indicator:checked, QCheckBox#captureExtrema::indicator:checked { image: url(:/images/checkbox-checked.svg); }
    )");
}
} // namespace Page5DialogStyle
