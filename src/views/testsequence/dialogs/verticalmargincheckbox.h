#pragma once
#include <QCheckBox>
#include <QVariantMap>

class VerticalMarginCheckBox : public QCheckBox {
public:
    explicit VerticalMarginCheckBox(const QVariantMap &settings, QWidget *parent = nullptr)
        : QCheckBox("Vertical margin (10% top/bottom)", parent)
    {
        setObjectName("verticalMargin");
        setChecked(settings.value("vertical_margin", false).toBool());
        setToolTip("上下各保留 10% 擷取量程，只自動擴大。截波仍以 Scale ×2 重新擷取。\n"
                   "未勾選：僅截波時 ×2。測項結束還原起始刻度，下次測試重新套用。");
    }
    void save(QVariantMap &settings) const { settings["vertical_margin"] = isChecked(); }
};
