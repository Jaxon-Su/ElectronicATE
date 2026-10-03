#pragma once
#include "tasksettingrules.h"
#include "dialogstyle.h"
#include <QDialog>
#include <QCheckBox>
#include <QFrame>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVariantMap>

class CaptureDialog : public QDialog {
  public:
    CaptureDialog(const QVariantMap& cfg, int seqNo, QWidget* parent, bool extrema = false) : QDialog(parent)
    {
        setWindowTitle(QString("Row %1 — Capture").arg(seqNo));
        setStyleSheet(Page5DialogStyle::withTypography(R"(
            QDialog { background: #f4f7fb; }
            QLabel { color: #2c3e50; }
            QLabel#titleLbl { font-weight: bold; color: #1a2a4a; }
            QLabel#taskBadge {
                font-weight: bold; color: white; background: #3a7bd5;
                border-radius: 8px; padding: 2px 8px;
            }
            QFrame#sep { color: #dce6f1; }
            QGroupBox {
                font-weight: bold; color: #2c3e6a;
                border: 1px solid #dce6f1; border-radius: 5px;
                margin-top: 8px; padding-top: 6px;
            }
            QGroupBox::title {
                subcontrol-origin: margin; left: 8px; top: 0px; padding: 0 4px;
            }
            QLineEdit, QSpinBox {
                background: white; border: 1px solid #c8d8ee;
                border-radius: 3px; padding: 2px 6px;
                min-height: 26px; color: #1a2a4a;
            }
            QLineEdit:focus, QSpinBox:focus { border-color: #3a7bd5; }
            QSpinBox { padding-right: 20px; }
            QSpinBox:disabled { color: #8899bb; background: #eef2f7; }
            QSpinBox::up-button {
                subcontrol-origin: border; subcontrol-position: top right;
                width: 18px; height: 14px;
            }
            QSpinBox::down-button {
                subcontrol-origin: border; subcontrol-position: bottom right;
                width: 18px; height: 14px;
            }
            QCheckBox { color: #1a2a4a; font-weight: 600; spacing: 8px; }
            QCheckBox::indicator { width: 16px; height: 16px; }
            QCheckBox::indicator:unchecked { image: url(:/images/checkbox-unchecked.svg); }
            QCheckBox::indicator:checked { image: url(:/images/checkbox-checked.svg); }
            QLabel#hintLbl { color: #52627a; }
            QPushButton#browseBtn {
                background: white; color: #1a2a4a; border: 1px solid #c8d8ee;
                border-radius: 3px; padding: 6px 12px;
            }
            QPushButton#browseBtn:hover { background: #e8f0fe; }
            QPushButton#applyBtn, QPushButton#closeBtn {
                color: white; border: none; border-radius: 4px;
                padding: 6px 24px; font-weight: bold;
            }
            QPushButton#applyBtn { background: #5a8cdd; }
            QPushButton#applyBtn:hover { background: #4a7ccd; }
            QPushButton#applyBtn:pressed { background: #3a6cbb; }
            QPushButton#closeBtn { background: #3a7bd5; }
            QPushButton#closeBtn:hover { background: #4a8be5; }
            QPushButton#closeBtn:pressed { background: #2a6bc5; }
        )"));
        setMinimumWidth(520);
        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(20, 16, 20, 16);
        root->setSpacing(10);
        auto* header = new QHBoxLayout;
        auto* title = new QLabel("Capture");
        title->setObjectName("titleLbl");
        auto* badge = new QLabel("CAPTURE");
        badge->setObjectName("taskBadge");
        header->addWidget(title);
        header->addStretch();
        header->addWidget(badge);
        root->addLayout(header);
        auto* separator = new QFrame;
        separator->setObjectName("sep");
        separator->setFrameShape(QFrame::HLine);
        root->addWidget(separator);
        auto* settings = new QGroupBox("Settings");
        auto* form = new QFormLayout(settings);
        form->setLabelAlignment(Qt::AlignRight);
        form->setHorizontalSpacing(16);
        form->setVerticalSpacing(12);
        root->addWidget(settings);
        auto* formatsRow = new QHBoxLayout;
        formatsRow->setSpacing(14);
        const auto selected = cfg.value("formats").toStringList();
        for (const QString& name : {"PNG", "CSV", "AllCSV", "WFM", "AllWFM"}) {
            auto* box = new QCheckBox(name);
            box->setChecked(selected.contains(name));
            formats.append(box);
            formatsRow->addWidget(box);
        }
        form->addRow("Formats:", formatsRow);
        directory = new QLineEdit(cfg.value("directory").toString());
        auto* browse = new QPushButton("Browse…");
        browse->setObjectName("browseBtn");
        auto* row = new QHBoxLayout;
        row->addWidget(directory);
        row->addWidget(browse);
        form->addRow("Save directory:", row);
        connect(browse, &QPushButton::clicked, this, [this] {
            const auto path = QFileDialog::getExistingDirectory(this, "Save directory", directory->text());
            if (!path.isEmpty())
                directory->setText(path);
        });
        channel = new QSpinBox;
        channel->setRange(TaskSettingRules::captureChannel.minimum, TaskSettingRules::captureChannel.maximum);
        channel->setSpecialValueText("Trigger Source");
        channel->setValue(cfg.value("channel", TaskSettingRules::captureChannel.initial).toInt());
        auto updateChannel = [this] {
            channel->setEnabled(formats[1]->isChecked() || formats[3]->isChecked());
        };
        for (auto* box : formats)
            connect(box, &QCheckBox::toggled, this, updateChannel);
        updateChannel();
        form->addRow("Channel:", channel);
        auto* help =
            new QLabel("PNG 保存畫面；CSV／WFM 保存指定通道（0 = Trigger Source）；AllCSV／AllWFM "
                       "保存所有已啟用通道，各通道一個檔案。\n自動產生唯一檔名，完成保存後才執行下一步。");
        if (extrema)
            help->setText("刷新 Max／Min 時立即保存；WFM／CSV 比較指定通道（0 = Trigger Source），"
                          "AllWFM／AllCSV 比較所有啟用通道。僅 PNG 時比較 Trigger Source。\n"
                          "保存位置留白則使用 Report File Path。JSON 記錄該次 Trigger Level 與量測值；"
                          "PNG 為畫面參考。最後 Result 指向獲勝檔案，歷次候選檔保留。");
        help->setWordWrap(true);
        help->setObjectName("hintLbl");
        form->addRow(help);
        root->addStretch();
        auto* buttons = new QHBoxLayout;
        auto* apply = new QPushButton("Apply");
        auto* close = new QPushButton("Close");
        apply->setObjectName("applyBtn");
        close->setObjectName("closeBtn");
        apply->setFixedWidth(100);
        close->setFixedWidth(100);
        buttons->addStretch();
        buttons->addWidget(apply);
        buttons->addWidget(close);
        root->addLayout(buttons);
        connect(apply, &QPushButton::clicked, this, &QDialog::accept);
        connect(close, &QPushButton::clicked, this, &QDialog::reject);
    }
    QVariantMap config() const
    {
        QStringList selected;
        for (auto* box : formats)
            if (box->isChecked())
                selected.append(box->text());
        return {
            {"directory", directory->text().trimmed()}, {"channel", channel->value()}, {"formats", selected}};
    }

  private:
    QLineEdit* directory;
    QSpinBox* channel;
    QList<QCheckBox*> formats;
};
