#include "styleutils.h"
#include <QTableWidget>
#include <QLineEdit>
#include <QStandardItemModel>

namespace StyleUtils {

void applyTableStyle(QTableWidget* tbl) {
    tbl->setStyleSheet(R"(
        QTableWidget { background-color: white; gridline-color: lightgray; border: none; }
        QHeaderView::section {
            background-color: #f0f0f0; border: 1px solid #dcdcdc;
            font-weight: bold; text-align: center; padding: 0; margin: 0;
        }
        QTableWidget::item { padding:0; margin:0; border:none; }
        QTableWidget::item:selected { background-color: white; color:black; }
    )");
}

void applyLineEditStyle(QLineEdit* le) {
    le->setStyleSheet(R"(
        QLineEdit { background-color: white; border:none; padding:0; margin:0; text-align:center; }
    )");
}

void applyComboBoxStyle(QComboBox *cb, bool headerLook)
{
    if (!cb) return;

    /* ------- 1) 將下拉項目置中 ------- */
    if (auto *m = qobject_cast<QStandardItemModel*>(cb->model())) {
        for (int i = 0; i < m->rowCount(); ++i)
            m->setData(m->index(i,0), Qt::AlignCenter, Qt::TextAlignmentRole);
    }

    /* ------- 2) 根據 headerLook 決定底色 ------- */
    const QString bg = headerLook ? "#f0f0f0"   // 與表頭同色
                                  : "white";

    cb->setStyleSheet(QString(R"(
        QComboBox               { background-color:%1; border:none;}
        QComboBox::drop-down    { width:0px;  border:none; }
        QComboBox::down-arrow   { width:0px; height:0px; }
        QComboBox QAbstractItemView::item { text-align:center; }
    )").arg(bg));
}

void applyHeaderLook(QWidget *w)
{
    if (!w) return;
    w->setStyleSheet(R"(
        background-color:#f0f0f0;
        font-weight:bold;
        qproperty-alignment:AlignCenter;
    )");
}

} // namespace StyleUtils


