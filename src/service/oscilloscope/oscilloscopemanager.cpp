#include "oscilloscopemanager.h"
#include "oscilloscope.h"
#include "icommunication.h"
#include "communicationfactory.h"
#include "oscilloscopefactory.h"
#include "abstracttriggercontroller.h"
#include <QThread>
#include <QDebug>

// ── 靜態：背景執行緒建立示波器 ──────────────────────────────────────────────

OscilloscopeManager::OscMap
OscilloscopeManager::buildFromConfig(const Page1Config& config)
{
    OscMap result;

    for (const auto& ic : std::as_const(config.instruments)) {
        if (ic.type != "Oscilloscope" || !ic.enabled) continue;
        if (ic.modelName.isEmpty() || ic.address.isEmpty())  continue;

        ICommunication* rawComm = CommunicationFactory::create(ic.address);
        if (!rawComm) continue;
        auto commPtr = std::shared_ptr<ICommunication>(rawComm);

        Oscilloscope* rawOsc =
            OscilloscopeFactory::createOscilloscope(ic.modelName, rawComm);
        if (!rawOsc) continue;  // commPtr 自動釋放

        // custom deleter：保證 comm 在 osc 之後才釋放
        auto oscPtr = std::shared_ptr<Oscilloscope>(rawOsc,
            [commPtr](Oscilloscope* osc) mutable {
                delete osc;
                commPtr.reset();
            });

        try {
            oscPtr->connect();
            if (!oscPtr->isConnected()) continue;
        } catch (const std::exception& e) {
            qWarning() << "[OscilloscopeManager] connect exception:" << e.what();
            continue;
        }

        result[ic.modelName] = oscPtr;
        qDebug() << "[OscilloscopeManager] created:" << ic.modelName;
    }

    return result;
}

void OscilloscopeManager::disconnectAll(OscMap& oscilloscopes)
{
    for (auto& osc : std::as_const(oscilloscopes)) {
        if (osc && osc->isConnected()) {
            try { osc->disconnect(); }
            catch (...) {}
        }
    }
    QThread::msleep(100);
    oscilloscopes.clear();
}

// ── 實例：主執行緒管理 ────────────────────────────────────────────────────────

void OscilloscopeManager::assign(OscMap newMap)
{
    m_map = std::move(newMap);
    m_current.reset();
    m_currentModel.clear();

    if (!m_map.isEmpty()) {
        auto it    = m_map.constBegin();
        m_current      = it.value();
        m_currentModel = it.key();
    }
}

void OscilloscopeManager::clear(AbstractTriggerController* triggerCtrl)
{
    if (triggerCtrl)
        triggerCtrl->setInstrument(nullptr);

    for (auto& osc : std::as_const(m_map)) {
        if (osc && osc->isConnected()) {
            try { osc->disconnect(); }
            catch (...) {}
        }
    }
    QThread::msleep(100);
    m_map.clear();
    m_current.reset();
    m_currentModel.clear();
}

std::shared_ptr<Oscilloscope>
OscilloscopeManager::get(const QString& modelName) const
{
    auto it = m_map.find(modelName);
    return (it != m_map.end()) ? *it : nullptr;
}

void OscilloscopeManager::setCurrent(const QString& modelName)
{
    auto osc = get(modelName);
    if (osc) {
        m_current      = osc;
        m_currentModel = modelName;
    }
}
