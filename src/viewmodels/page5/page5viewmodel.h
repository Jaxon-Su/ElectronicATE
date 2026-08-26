#pragma once
#include <QObject>
#include <QVector>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include "page1config.h"
#include "page2config.h"
#include "page5model.h"
#include "page5taskpayload.h"
#include "page5runpanel.h"      // Page5RunPanel::TaskStatus
#include "ixmlserializable.h"

class Page5Model;
class Page2ViewModel;
class Oscilloscope;
class Page5TestWorker;
class QThread;

class Page5ViewModel : public QObject, public IXmlSerializable
{
    Q_OBJECT
public:
    explicit Page5ViewModel(Page5Model *model, QObject *parent = nullptr);
    ~Page5ViewModel() override;

    void setPage2ViewModel(Page2ViewModel* vm2) { m_page2ViewModel = vm2; }

    const Page1Config&             page1Config()  const;
    const QVector<InputRow>&       inputRows()    const;
    const LoadMetaRow&             loadMeta()     const;
    const QVector<LoadDataRow>&    loadRows()     const;
    const DynamicMetaRow&          dynamicMeta()  const;
    const QVector<DynamicDataRow>& dynamicRows()  const;
    const QVector<RelayDataRow>&   relayRows()    const;
    const QVector<DutRowData>&     dutRows()      const;

    Oscilloscope* oscilloscope() const { return m_oscilloscope; }
    void setOscilloscope(Oscilloscope* scope) { m_oscilloscope = scope; }


    QString xmlTagName() const override { return "Page5"; }
    void writeXml(QXmlStreamWriter& writer) const override;
    void loadXml(QXmlStreamReader& reader) override;

    bool isRunning() const { return m_isRunning; }

public slots:
    void onPage1ConfigChanged(const Page1Config &cfg);
    void onInputDataChanged(const QVector<InputRow>&);
    void onLoadMetaChanged(const LoadMetaRow&);
    void onLoadRowsChanged(const QVector<LoadDataRow>&);
    void onDynamicMetaChanged(const DynamicMetaRow&);
    void onDynamicRowsChanged(const QVector<DynamicDataRow>&);
    void onRelayRowsChanged(const QVector<RelayDataRow>&);
    void onDutRowsChanged(const QVector<DutRowData>& rows);

    // ★ 執行控制：由 View 呼叫，Worker 生命週期由 ViewModel 管理
    void startExecution(const QVector<TaskPayload>& payloads);
    void stopExecution();

    void broadcastAllData();
    void onConfigLoaded();

signals:
    void inputTableChanged();
    void loadTableChanged();
    void dynamicTableChanged();
    void relayTableChanged();
    void dutTableChanged();
    void runningChanged(bool running);

    // ★ Worker 輸出轉發給 View（queued 跨執行緒後再由 ViewModel 廣播）
    void taskStatusChanged(int runPanelIndex, Page5RunPanel::TaskStatus status);
    void taskRetryCountChanged(int runPanelIndex, int attempt);
    void logMessage(const QString& msg);

private slots:
    void setRunning(bool running);

private:
    Page5Model*                   m_model          = nullptr;
    Page2ViewModel*               m_page2ViewModel = nullptr;
    Oscilloscope*                 m_oscilloscope   = nullptr;
    Page5TestWorker* m_worker       = nullptr;
    QThread*         m_workerThread = nullptr;
    bool m_hasPage1Config                          = false;
    bool m_isRunning                               = false;
};
