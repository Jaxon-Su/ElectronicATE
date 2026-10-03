#pragma once
#include "../../service/console/chroma63600commands.h"
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

class Chroma63600QuickDialog : public QDialog
{
public:
    explicit Chroma63600QuickDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("63600 Series — Quick Commands");
        setMinimumWidth(440);
        auto *layout = new QVBoxLayout(this);
        auto *form = new QFormLayout;
        channel = new QSpinBox(this);
        channel->setObjectName("channel");
        channel->setRange(1, 10);
        mode = new QComboBox(this);
        mode->setObjectName("slopeMode");
        mode->addItems({"Static", "Dynamic", "Both"});
        auto number = [this](const QString &name, double minimum, double value, const QString &unit) {
            auto *spin = new QDoubleSpinBox(this);
            spin->setObjectName(name);
            spin->setDecimals(6);
            spin->setRange(minimum, 1000000);
            spin->setValue(value);
            spin->setSuffix(unit);
            return spin;
        };
        rise = number("riseSlope", 0.000001, 1, " A/µs");
        fall = number("fallSlope", 0.000001, 1, " A/µs");
        von = number("von", 0, 0, " V");
        form->addRow("Channel:", channel);
        form->addRow("Slope settings:", mode);
        form->addRow("Rise slope:", rise);
        form->addRow("Fall slope:", fall);
        form->addRow("Von:", von);
        layout->addLayout(form);
        auto *hint = new QLabel(tr("Channel 為儀器實體通道；63640 通道使用 1、3、5、7、9。\n"
                                  "斜率與 Von 的有效範圍依模組及檔位而定。\n"
                                  "填入後按 Page4 Send 送出；不切換負載模式或 LOAD ON。"), this);
        hint->setWordWrap(true);
        layout->addWidget(hint);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
        auto *insert = buttons->addButton("Insert Commands", QDialogButtonBox::AcceptRole);
        connect(insert, &QPushButton::clicked, this, &QDialog::accept);
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        layout->addWidget(buttons);
    }
    QString commands() const
    {
        return Chroma63600Commands::build(channel->value(),
            static_cast<Chroma63600Commands::SlopeMode>(mode->currentIndex()),
            rise->value(), fall->value(), von->value());
    }
private:
    QSpinBox *channel;
    QComboBox *mode;
    QDoubleSpinBox *rise, *fall, *von;
};
