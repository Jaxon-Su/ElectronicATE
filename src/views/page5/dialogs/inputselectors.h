#pragma once
#include "inputselection.h"
#include <QComboBox>
#include <QFormLayout>
#include <QLabel>

// Independent choices: selecting DC never clears AC, and vice versa.
class InputSelectors final : public QObject {
  public:
    InputSelectors(QFormLayout *form, const InputConditionOptions &options, const QVariantMap &cfg)
        : QObject(form->parentWidget())
    {
        ac = add(form, "AC Input :", "acInputCombo", options.ac, cfg.value("ac_input_index", -1).toInt());
        dc = add(form, "DC Input :", "dcInputCombo", options.dc, cfg.value("dc_input_index", -1).toInt());
        auto *preview = new QLabel;
        preview->setObjectName("previewLbl");
        preview->setWordWrap(true);
        form->addRow("", preview);
        const auto refresh = [this, preview] {
            QStringList selected;
            if (ac->currentData().toInt() >= 0) selected << "AC: " + ac->currentText();
            if (dc->currentData().toInt() >= 0) selected << "DC: " + dc->currentText();
            preview->setText(selected.isEmpty() ? QString::fromUtf8("—") : selected.join('\n'));
        };
        connect(ac, &QComboBox::currentIndexChanged, this, refresh);
        connect(dc, &QComboBox::currentIndexChanged, this, refresh);
        refresh();
    }
    void save(QVariantMap &cfg) const
    {
        for (const auto &entry : {qMakePair(QString("ac_input"), ac), qMakePair(QString("dc_input"), dc)}) {
            cfg[entry.first + "_index"] = entry.second->currentData();
            cfg[entry.first + "_label"] = entry.second->currentData().toInt() >= 0
                ? entry.second->currentText() : QString();
        }
    }
    QComboBox *ac, *dc;
  private:
    static QComboBox *add(QFormLayout *form, const QString &label, const char *name,
                         const QStringList &options, int index)
    {
        auto *combo = new QComboBox;
        combo->setObjectName(name);
        combo->addItem(QString::fromUtf8("— None —"), -1);
        for (int i = 0; i < options.size(); ++i)
            if (!options[i].trimmed().isEmpty()) combo->addItem(options[i], i);
        combo->setCurrentIndex(qMax(0, combo->findData(index)));
        form->addRow(label, combo);
        return combo;
    }
};
