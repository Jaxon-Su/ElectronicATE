#pragma once
#include <QDialog>

class QLabel;
class QPushButton;

class CustomErrorDialog : public QDialog {
    Q_OBJECT
public:
    CustomErrorDialog(const QString& title, const QString& msg, QWidget* parent = nullptr);

private:
    QLabel* label;
    QPushButton* btn;
};
