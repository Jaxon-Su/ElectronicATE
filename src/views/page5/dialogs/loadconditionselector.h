#pragma once
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QVariantMap>
#include <QVBoxLayout>

class LoadConditionSelector : public QWidget
{
  public:
    enum class Selection { Single, Group };

    LoadConditionSelector(const QStringList &options, QString key, Selection selection,
                          QWidget *parent = nullptr)
        : QWidget(parent), m_key(std::move(key))
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);
        auto *preview = new QLabel(this);
        preview->setObjectName("previewLbl");
        preview->setWordWrap(true);
        if (selection == Selection::Group) {
            m_list = new QListWidget(this);
            m_list->setObjectName(m_key == "dyload" ? "dynamicConditions" : "loadConditions");
            m_list->setSelectionMode(QAbstractItemView::NoSelection);
            m_list->setFixedHeight(150);
            m_list->setStyleSheet(
                "QListWidget { background: white; color: #1a2a4a; border: 1px solid #c8d8ee; }"
                "QListWidget::item { padding: 5px; }"
                "QListWidget::indicator { width: 16px; height: 16px; }"
                "QListWidget::indicator:unchecked { image: url(:/images/checkbox-unchecked.svg); }"
                "QListWidget::indicator:checked { image: url(:/images/checkbox-checked.svg); }");
            for (int i = 0; i < options.size(); ++i) {
                if (options[i].trimmed().isEmpty())
                    continue;
                auto *item = new QListWidgetItem(QString("%1.  %2").arg(i + 1).arg(options[i]), m_list);
                item->setData(Qt::UserRole, i);
                item->setData(Qt::UserRole + 1, options[i]);
                item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                item->setCheckState(Qt::Unchecked);
            }
            auto updateCount = [this, preview] {
                int count = 0;
                for (int i = 0; i < m_list->count(); ++i)
                    count += m_list->item(i)->checkState() == Qt::Checked;
                preview->setText(QString("%1 selected · Page2 order").arg(count));
            };
            connect(m_list, &QListWidget::itemChanged, this, updateCount);
            updateCount();
            layout->addWidget(m_list);
        } else {
            m_combo = new QComboBox(this);
            m_combo->addItem("— None —", -1);
            for (int i = 0; i < options.size(); ++i)
                if (!options[i].trimmed().isEmpty()) {
                    m_combo->addItem(QString("%1.  %2").arg(i + 1).arg(options[i]), i);
                    m_combo->setItemData(m_combo->count() - 1, options[i], Qt::UserRole + 1);
                }
            auto updatePreview = [this, preview] {
                preview->setText(m_combo->currentData().toInt() < 0 ? "—"
                    : m_combo->currentData(Qt::UserRole + 1).toString());
            };
            connect(m_combo, &QComboBox::currentIndexChanged, this, updatePreview);
            updatePreview();
            layout->addWidget(m_combo);
        }
        layout->addWidget(preview);
    }

    void restore(const QVariantMap &config)
    {
        if (m_combo) {
            m_combo->setCurrentIndex(qMax(0, m_combo->findData(config.value(m_key + "_index", -1))));
            return;
        }
        const auto indices = config.value(m_key + "_indices").toList();
        for (int i = 0; i < m_list->count(); ++i) {
            auto *item = m_list->item(i);
            bool selected = false;
            for (const auto &index : indices)
                selected |= index.toInt() == item->data(Qt::UserRole).toInt();
            item->setCheckState(selected ? Qt::Checked : Qt::Unchecked);
        }
    }

    void save(QVariantMap &config) const
    {
        if (m_combo) {
            config[m_key + "_index"] = m_combo->currentData();
            config[m_key + "_label"] = m_combo->currentData(Qt::UserRole + 1).toString();
            return;
        }
        QVariantList indices;
        QStringList labels;
        for (int i = 0; i < m_list->count(); ++i) {
            const auto *item = m_list->item(i);
            if (item->checkState() == Qt::Checked) {
                indices.append(item->data(Qt::UserRole));
                labels.append(item->data(Qt::UserRole + 1).toString());
            }
        }
        config[m_key + "_indices"] = indices;
        config[m_key + "_labels"] = labels;
    }

  private:
    QString m_key;
    QComboBox *m_combo = nullptr;
    QListWidget *m_list = nullptr;
};
