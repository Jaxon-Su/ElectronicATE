#include "page4viewmodel.h"
#include "page4model.h"

Page4ViewModel::Page4ViewModel(Page4Model* model, QObject *parent)
    : QObject(parent)
    , m_page4Model(model)
{

}

Page4ViewModel::~Page4ViewModel()
{

}
