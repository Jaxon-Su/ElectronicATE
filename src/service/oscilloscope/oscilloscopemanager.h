#pragma once
#include <memory>
#include <QMap>
#include <QString>
#include <QStringList>
#include "page1config.h"

class Oscilloscope;
class AbstractTriggerController;

// 示波器生命週期管理
// - 靜態方法供背景執行緒使用（建立 / 斷線）
// - 實例方法供主執行緒管理「當前活躍示波器」
class OscilloscopeManager {
public:
    using OscMap = QMap<QString, std::shared_ptr<Oscilloscope>>;

    // ── 靜態（可在任何執行緒呼叫）────────────────────────────────────────
    // 依 Page1Config 建立所有啟用的示波器，回傳 modelName → shared_ptr 的 map
    static OscMap buildFromConfig(const Page1Config& config);

    // 斷線 map 內所有儀器並清空（呼叫後 map 為空）
    static void disconnectAll(OscMap& oscilloscopes);

    // ── 實例（主執行緒使用）──────────────────────────────────────────────
    // 以新 map 取代舊的（同時更新 current 為第一台）
    void assign(OscMap newMap);

    // 斷線並清除所有儀器；若傳入 triggerCtrl 會先解除綁定
    void clear(AbstractTriggerController* triggerCtrl = nullptr);

    // 依名稱取得示波器
    std::shared_ptr<Oscilloscope> get(const QString& modelName) const;

    // 設定當前示波器（connectTriggerController 呼叫）
    void setCurrent(const QString& modelName);

    std::shared_ptr<Oscilloscope> current()     const { return m_current; }
    QString                       currentModel() const { return m_currentModel; }
    bool                          isEmpty()      const { return m_map.isEmpty(); }
    QStringList                   modelNames()   const { return m_map.keys(); }

private:
    OscMap                        m_map;
    std::shared_ptr<Oscilloscope> m_current;
    QString                       m_currentModel;
};
