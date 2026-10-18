#pragma once
#include "capturedialog.h"

class StrategyCaptureWidget : public QWidget {
public:
    explicit StrategyCaptureWidget(const QVariantMap& cfg, QWidget* parent = nullptr) : QWidget(parent)
    {
        settings = cfg.value("capture_settings").toMap();
        if (settings.isEmpty()) settings = {{"formats", QStringList{"WFM", "PNG"}}, {"channel", 0}};
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        enabled = new QCheckBox("Capture every acquisition", this);
        enabled->setObjectName("captureEnabled");
        enabled->setChecked(cfg.value("capture_enabled", false).toBool());
        enabled->setToolTip("Save every completed acquisition before the next one. Empty directory uses Report File Path.");
        auto* edit = new QPushButton("Capture settings…", this);
        edit->setEnabled(enabled->isChecked());
        layout->addWidget(enabled);
        layout->addWidget(edit);
        connect(enabled, &QCheckBox::toggled, edit, &QWidget::setEnabled);
        connect(edit, &QPushButton::clicked, this, [this] {
            CaptureDialog dialog(settings, this);
            dialog.setWindowTitle("Capture — every acquisition");
            if (dialog.exec() == QDialog::Accepted) settings = dialog.config();
        });
    }
    void save(QVariantMap& cfg) const
    {
        cfg["capture_enabled"] = enabled->isChecked();
        cfg["capture_settings"] = settings;
    }
private:
    QCheckBox* enabled;
    QVariantMap settings;
};
