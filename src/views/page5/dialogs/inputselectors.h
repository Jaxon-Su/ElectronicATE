#pragma once
#include <QComboBox>
#include <QFormLayout>
#include <QSignalBlocker>

// Keep one saved condition index while presenting separate, mutually exclusive AC/DC choices.
inline void addInputSelectors(QFormLayout* form, QComboBox* selection, const QStringList& options)
{
    selection->setParent(form->parentWidget());
    selection->hide();
    auto* ac = new QComboBox;
    auto* dc = new QComboBox;
    ac->setObjectName("acInputCombo");
    dc->setObjectName("dcInputCombo");
    ac->addItem(QString::fromUtf8("— None —"), -1);
    dc->addItem(QString::fromUtf8("— None —"), -1);
    for (int i = 0; i < options.size(); ++i) {
        if (options[i].trimmed().isEmpty())
            continue;
        const bool isDc = options[i].startsWith("DC: ");
        auto* combo = isDc ? dc : ac;
        combo->addItem(isDc ? options[i].mid(4) : options[i], i);
    }
    form->addRow("AC Input :", ac);
    form->addRow("DC Input :", dc);
    const auto refresh = [selection, ac, dc] {
        const QSignalBlocker blockAc(ac), blockDc(dc);
        const int selected = selection->currentData().toInt();
        ac->setCurrentIndex(qMax(0, ac->findData(selected)));
        dc->setCurrentIndex(qMax(0, dc->findData(selected)));
    };
    QObject::connect(selection, &QComboBox::currentIndexChanged, selection, refresh);
    for (auto* combo : {ac, dc})
        QObject::connect(combo, &QComboBox::currentIndexChanged, selection, [selection, combo] {
            selection->setCurrentIndex(qMax(0, selection->findData(combo->currentData())));
        });
    refresh();
}
