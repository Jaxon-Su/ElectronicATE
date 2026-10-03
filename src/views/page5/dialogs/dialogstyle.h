#pragma once

#include <QString>

namespace Page5DialogStyle {
// Shared typography and checkbox indicators for all Page5 dialogs.
inline QString withTypography(const char* style)
{
    return QString::fromUtf8(style) + QStringLiteral(R"(
        QWidget { font-family: "Segoe UI"; font-size: 12px; }
        QLabel#titleLbl { font-size: 15px; font-weight: bold; }
        QCheckBox { color: #1a2a4a; spacing: 8px; }
        QCheckBox:disabled { color: #8993a7; }
        QCheckBox::indicator { width: 16px; height: 16px; }
        QCheckBox::indicator:unchecked { image: url(:/images/checkbox-unchecked.svg); }
        QCheckBox::indicator:checked { image: url(:/images/checkbox-checked.svg); }
    )");
}
} // namespace Page5DialogStyle
