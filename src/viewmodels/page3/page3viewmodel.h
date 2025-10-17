#pragma once

#include <QObject>
#include <QStringList>
#include <QMap>
#include "page3model.h"
#include "page1config.h"
#include "page2config.h"
#include "dcload.h"
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include "oscilloscope.h"
#include "oscilloscopefactory.h"
#include "abstracttriggercontroller.h"

enum class InputAction { PowerOn, PowerOff, Change };
enum class LoadAction { LoadOn, LoadOff, Change };
enum class DyLoadAction { DyLoadOn, DyloadOff, Change };

class Page3ViewModel : public QObject
{
    Q_OBJECT
public:
    Page3ViewModel(Page3Model* p3, QObject *parent = nullptr);
    virtual ~Page3ViewModel();

    void writeXml(QXmlStreamWriter& writer) const;
    void loadXml(QXmlStreamReader& reader);

    void restoreFromModel();
    void updateUIAfterLoad();
    void restoreUISelections();

    // 獲取當前選擇狀態的方法
    int getSelectedInputIndex() const { return m_selectedInputIndex; }
    QString getSelectedInputText() const { return m_selectedInputText; }
    int getSelectedLoadIndex() const { return m_selectedLoadIndex; }
    QString getSelectedLoadText() const { return m_selectedLoadText; }
    int getSelectedDyLoadIndex() const { return m_selectedDyLoadIndex; }
    QString getSelectedDyLoadText() const { return m_selectedDyLoadText; }
    int getSelectedRelayIndex() const { return m_selectedRelayIndex; }
    QString getSelectedRelayText() const { return m_selectedRelayText; }

    // 負載設定方法
    void applyLoadSettings(DCLoad* dcLoad,
                           int index,
                           double currval,
                           const QString& mode,
                           const QVector<QString>& vons,
                           const QVector<QString>& riseSlope,
                           const QVector<QString>& fallSlope,
                           const QVector<QString>& outputVoltages);

    void applyDyLoadSettings(DCLoad* dcLoad,
                             int index,
                             const QString& currval,
                             const QString& dyTime,
                             const QString& mode,
                             const QVector<QString>& vons,
                             const QVector<QString>& riseSlope,
                             const QVector<QString>& fallSlope,
                             const QVector<QString>& outputVoltages);

public slots:
    void setMaxOutput(int maxOutput);
    void setNameList(const QStringList &names);
    void updateTitles(LoadKind, const QStringList& titles);

    // Page1/Page2 數據處理
    void onPage1ConfigChanged(const Page1Config &cfg);
    void onInputDataChanged(const QVector<InputRow>& rows);
    void onLoadMetaChanged(const LoadMetaRow& meta);
    void onLoadRowsChanged(const QVector<LoadDataRow>& rows);
    void onDynamicMetaChanged(const DynamicMetaRow& meta);
    void onDynamicRowsChanged(const QVector<DynamicDataRow>& rows);

    // Input 相關操作
    void onInputToggled(bool on);
    void onInputChanged();
    void handleInput(InputAction action);

    // Load 相關操作
    void onLoadToggled(bool on);
    void onLoadChanged();
    void handleLoad(LoadAction action);

    // Dynamic Load 相關操作
    void onDyloadToggled(bool on);
    void onDyLoadChanged();
    void handleDyLoad(DyLoadAction action);

    // 選擇處理
    void onSelected(LoadKind type, int idx, const QString& txt);

    // Trigger 相關
    void onTriggerWidgetCreated(const QString& modelName, QObject* triggerController);
    void onTriggerWidgetDestroyed();

private:
    // UI 狀態
    QStringList m_names;
    QStringList m_inputTitles;
    QStringList m_loadTitles;
    QStringList m_dyloadTitles;
    QStringList m_relayTitles;

    // 數據模型
    Page3Model* m_page3 = nullptr;

    // 配置數據
    Page1Config m_page1Config;
    QVector<InputRow> m_page2InputData;
    LoadMetaRow m_LoadMetaData;
    QVector<LoadDataRow> m_LoadRowsData;
    DynamicMetaRow m_DynamicMetaData;
    QVector<DynamicDataRow> m_DynamicRowsData;

    // 當前選擇狀態
    int m_selectedInputIndex = -1;
    int m_selectedLoadIndex = -1;
    int m_selectedDyLoadIndex = -1;
    int m_selectedRelayIndex = -1;
    QString m_selectedRelayText;
    QString m_selectedInputText;
    QString m_selectedLoadText;
    QString m_selectedDyLoadText;

    // === 抽象化的儀器管理 ===
    QMap<QString, Oscilloscope*> m_oscilloscopes;
    Oscilloscope* m_currentOscilloscope = nullptr;
    AbstractTriggerController* m_currentTriggerController = nullptr;
    QString m_currentInstrumentModel;

    // 私有方法
    Oscilloscope* getOscilloscopeByModel(const QString& modelName);
    void createAllInstruments();
    void cleanupAllInstruments();
    void createOscilloscopes();
    void cleanupOscilloscopes();
    void connectTriggerController();
    void cleanupTriggerResources();

signals:
    void headersChanged(const QStringList &hdr);
    void rowLabelsChanged(const QStringList &lbl);
    void page1ConfigChanged(const Page1Config &cfg);
    void TitlesUpdated(LoadKind type, const QStringList& titles);
    void forceOff(LoadKind type);
    void restoreSelections(LoadKind type, int index, const QString& text);
};

// OscilloscopeFactory → 創建 DPO7000 示波器物件
// TriggerWidgetFactory → 創建 DPO7000 的 UI 控制界面
// DPO7000TriggerController → DPO7000 專用的控制邏輯
// AbstractTriggerController → 抽象基類，統一介面

// connect(vm1, &Page1ViewModel::configUpdated,
//         vm3, &Page3ViewModel::onPage1ConfigChanged);

// >> Page3ViewModel::onPage1ConfigChanged
//     >> createAllInstruments();(cleanupAllInstruments();createOscilloscopes();)
//     emit page1ConfigChanged
// >>connect(vm, &Page3ViewModel::page1ConfigChanged,
//                this, &Page3::onPage1ConfigChanged);

// >>Page3::onPage1ConfigChanged >Page3::>setTriggerModel >> Page3::createTriggerWidget
