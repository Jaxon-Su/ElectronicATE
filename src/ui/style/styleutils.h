#pragma once

class QTableWidget;
class QLineEdit;
#include <QComboBox>

namespace StyleUtils {
void applyTableStyle(QTableWidget* tbl);
void applyLineEditStyle(QLineEdit* le);
void applyComboBoxStyle(QComboBox *cb, bool headerLook = false);
void applyHeaderLook(QWidget *w);
}
