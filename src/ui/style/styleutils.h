#pragma once

class QTableWidget;
class QTableView;
class QLineEdit;
#include <QComboBox>

namespace StyleUtils
{
void applyTableStyle(QTableView *tbl);
void applyLineEditStyle(QLineEdit *le);
void applyComboBoxStyle(QComboBox *cb, bool headerLook = false);
void applyHeaderLook(QWidget *w);
} // namespace StyleUtils
