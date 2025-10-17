#pragma once
#include <QObject>

class Page4Model;

class Page4ViewModel : public QObject
{
    Q_OBJECT

public:
    explicit Page4ViewModel(Page4Model* model, QObject *parent = nullptr);
    ~Page4ViewModel();

private:
    Page4Model* m_page4Model = nullptr;
};
