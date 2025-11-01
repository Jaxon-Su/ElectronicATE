#pragma once

#include <QObject>
#include <QStringList>
#include <QMap>
#include "page3model.h"
#include "page1config.h"
#include "page2config.h"
#include "dcload.h"
#include "acsource.h"
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include "oscilloscope.h"
#include "abstracttriggercontroller.h"
#include <QMutex>

enum class InputAction { PowerOn, PowerOff, Change };
enum class LoadAction { LoadOn, LoadOff, Change };
enum class DyLoadAction { DyLoadOn, DyloadOff, Change };
enum class ConfigUpdateState {Idle,Pending,Processing};

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
                           double value,
                           const QString& mode,
                           const QVector<QString>& vons,
                           const QVector<QString>& riseSlopeCCH,
                           const QVector<QString>& fallSlopeCCH,
                           const QVector<QString>& riseSlopeCCL,
                           const QVector<QString>& fallSlopeCCL,
                           const QVector<QString>& outputVoltages);


    void applyLoadVonSetting(DCLoad* dcLoad,
                             const int &index,
                             const QVector<QString>& vons);

    void applyLoadSlopeSetting(DCLoad* dcLoad,
                               int index,
                               const QVector<QString>& riseSlopeCCH,
                               const QVector<QString>& fallSlopeCCH,
                               const QVector<QString>& riseSlopeCCL,
                               const QVector<QString>& fallSlopeCCL);

    void applyLoadValueSettings(DCLoad* dcLoad,
                           int index,
                           double value,
                           const QString& mode,
                           const QVector<QString>& outputVoltages);


    void applyDyLoadSettings(DCLoad* dcLoad,
                             int index,
                             const QString& value,
                             const QString& dyTime,
                             const QVector<QString>& vons,
                             const QVector<QString>& riseSlopeCCDH,
                             const QVector<QString>& fallSlopeCCDH,
                             const QVector<QString>& riseSlopeCCDL,
                             const QVector<QString>& fallSlopeCCDL,
                             const QVector<QString>& outputVoltages);

    void applyDyLoadSlopeSetting(DCLoad* dcLoad,
                               int index,
                               const QVector<QString>& riseSlopeCCDH,
                               const QVector<QString>& fallSlopeCCDH,
                               const QVector<QString>& riseSlopeCCDL,
                               const QVector<QString>& fallSlopeCCDL);

    void applyDyLoadValueSettings(DCLoad* dcLoad,
                             int index,
                             const QString& value,
                             const QString& dyTime,
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


private slots:

    // force off
    void emitForceOff(LoadKind kind);
    void applyPendingConfig();

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
    QMap<QString, ICommunication*> m_oscilloscopeComms;
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

    // handleInput相關輔助函數
    bool validateInputConfiguration();

    struct ACSourceCreationResult {
        ACSource* source = nullptr;
        ICommunication* comm = nullptr;
        bool success = false;
    };

    ACSourceCreationResult createACSource(const Page1Config& cfg, QPointer<Page3ViewModel> self);

    struct InputParameters {
        double voltage = 0.0;
        double frequency = 0.0;
        double phase = 0.0;
        bool valid = false;
    };
    InputParameters parseInputText(const QString& inputText);

    void executeACSourceAction(ACSource* source, InputAction action, const InputParameters& params);
    void cleanupACSourceResources(ACSource* source, ICommunication* comm);

    // handleLoad 相關輔助函數
    bool validateLoadConfiguration();

    struct LoadDataInfo {
        QVector<QString> values;
        bool found = false;
    };

    LoadDataInfo findSelectedLoadData(QPointer<Page3ViewModel> self);

    void executeDCLoadAction(
        DCLoad* dcLoad,
        LoadAction action,
        int index,
        const LoadDataInfo& dataInfo,
        const QVector<QString>& modes,
        const QVector<QString>& vons,
        const QVector<QString>& riseSlopeCCH,
        const QVector<QString>& fallSlopeCCH,
        const QVector<QString>& riseSlopeCCL,
        const QVector<QString>& fallSlopeCCL,
        const QVector<QString>& outputVoltages);



    // handleDyLoad 相關輔助函數

    bool validateDyLoadConfiguration();

    struct DyLoadDataInfo {
        QVector<QString> values;
        QString t1t2;
        bool found = false;
    };
    DyLoadDataInfo findSelectedDyLoadData(QPointer<Page3ViewModel> self,
                                          const QVector<QString>& t1t2Vector);

    void executeDCDyLoadAction(
        DCLoad* dcLoad,
        DyLoadAction action,
        int index,
        const DyLoadDataInfo& dataInfo,
        const QVector<QString>& vons,
        const QVector<QString>& riseSlopeCCDH,
        const QVector<QString>& fallSlopeCCDH,
        const QVector<QString>& riseSlopeCCDL,
        const QVector<QString>& fallSlopeCCDL,
        const QVector<QString>& outputVoltages);

    // handleLoad、handleDyLoad共用

    struct DCLoadCreationResult {
        QVector<DCLoad*> dcLoads;
        QMap<QString, ICommunication*> commMap;
        bool success = false;
    };

    void cleanupDCLoadResources(
        QVector<DCLoad*>& dcLoads,
        QMap<QString, ICommunication*>& commMap);

    DCLoadCreationResult createDCLoads(const Page1Config& cfg,
                                       QPointer<Page3ViewModel> self,
                                       LoadKind kind);

     // 防抖計時器
     // 當配置變更時，不立即執行，而是啟動計時器。
     // 如果在計時期間又有新的配置變更，會重置計時器。
     // 只有當計時器到期時，才執行實際的配置更新。
        QTimer* m_configTimer = nullptr;

    // 暫存待處理的配置
    // 每次收到新配置時，都會更新這個變數。
    // 當防抖計時器到期時，使用這個配置進行更新。
        Page1Config m_pendingConfig;


    // 配置更新狀態
    // 追蹤當前的配置更新狀態，避免在處理中時重複觸發。
        ConfigUpdateState m_updateState = ConfigUpdateState::Idle;

    //狀態保護互斥鎖
    //保護 m_updateState 的讀寫，確保執行緒安全。
        mutable QMutex m_stateMutex;

     //防抖延遲時間（毫秒）
        static const int configDelayTime = 500;



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
