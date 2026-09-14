#include "oscilloscopemanager.h"
#include "oscilloscope.h"
#include <QThread>
#include <QDebug>
#include <utility>


void OscilloscopeManager::disconnectAll(OscMap& oscilloscopes)
{
    for (auto& osc : std::as_const(oscilloscopes)) {
        if (!osc) continue;
        try {
            if (osc->isConnected()) osc->disconnect();
        } catch (...) {
            qWarning() << "[OscilloscopeManager] disconnect failed; continuing cleanup";
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

void OscilloscopeManager::clear()
{
    disconnectAll(m_map);
    m_current.reset();
    m_currentModel.clear();
}

OscilloscopeManager::OscMap OscilloscopeManager::takeAll()
{
    m_current.reset();
    m_currentModel.clear();
    return std::exchange(m_map, {});
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
