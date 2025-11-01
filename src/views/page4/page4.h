#pragma once

#include <QWidget>

class Page4ViewModel;  // 前向聲明

class Page4 : public QWidget
{
    Q_OBJECT

public:
    explicit Page4(Page4ViewModel* viewModel, QWidget *parent = nullptr);
    ~Page4() override = default;

private:
    Page4ViewModel* vm = nullptr;
    void setupUI();
};
