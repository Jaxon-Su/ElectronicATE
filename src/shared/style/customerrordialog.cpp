#include "customerrordialog.h"
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QIcon>

CustomErrorDialog::CustomErrorDialog(const QString& title, const QString& msg, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(title);
    setFixedSize(350, 130);
    setWindowIcon(QIcon(":/images/error.png"));

    QVBoxLayout* layout = new QVBoxLayout(this);
    label = new QLabel(msg, this);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet("font-size:14px; padding:10px 0;");
    label->setWordWrap(false);

    btn = new QPushButton("OK", this);
    btn->setMinimumHeight(20);
    btn->setFixedWidth(190);
    btn->setStyleSheet("font-size:12px;");

    layout->addWidget(label, 1, Qt::AlignCenter);
    layout->addWidget(btn, 0, Qt::AlignHCenter);

    connect(btn, &QPushButton::clicked, this, &QDialog::accept);
}
