#include "triggerbinding.h"
#include "triggermodelcatalog.h"

TriggerBinding::~TriggerBinding()
{
    detach();
}

bool TriggerBinding::attach(ITriggerController* controller, const QString& modelName)
{
    detach();
    if (!controller || TriggerModelCatalog::familyName(controller->getSupportedModel())
                           != TriggerModelCatalog::familyName(modelName))
        return false;
    m_controller = controller;
    m_modelName = modelName;
    m_reconnect = connect(controller, &ITriggerController::reconnectRequested,
                          this, &TriggerBinding::reconnectRequested);
    return true;
}

void TriggerBinding::detach()
{
    disconnect(m_reconnect);
    m_reconnect = {};
    if (m_controller) m_controller->setInstrument(nullptr);
    m_controller.clear();
    m_modelName.clear();
}

void TriggerBinding::bindInstrument(Oscilloscope* instrument)
{
    if (m_controller) m_controller->setInstrument(instrument);
}

int TriggerBinding::selectedChannel() const
{
    return m_controller ? m_controller->getSelectedChannel() : 0;
}
