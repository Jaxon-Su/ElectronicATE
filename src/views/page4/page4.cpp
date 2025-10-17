#include "page4.h"
#include "page4viewmodel.h"
#include <QVBoxLayout>
#include <QLabel>

Page4::Page4(Page4ViewModel* viewModel, QWidget *parent)
    : QWidget(parent), vm(viewModel)
{
    // setupUI();
}

void Page4::setupUI()
{
    // QVBoxLayout *layout = new QVBoxLayout(this);
    // QLabel *label = new QLabel("Page 4 - Tasks Content", this);
    // label->setAlignment(Qt::AlignCenter);
    // label->setStyleSheet("font-size: 18px; color: blue;");
    // layout->addWidget(label);
}
