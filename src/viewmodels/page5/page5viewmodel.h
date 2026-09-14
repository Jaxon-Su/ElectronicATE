#pragma once
#include <QObject>
#include <QVector>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include "page1config.h"
#include "page2config.h"
#include "dutrowdata.h"
#include "page5taskpayload.h"
#include "taskstatus.h"
#include "ixmlserializable.h"

class Page5Model;
class ITestConditionProvider;
class Oscilloscope;
class Page5TestWorker;
class QThread;

class Page5ViewModel : public QObject, public IXmlSerializable
{
    Q_OBJECT
public:
    void setControlAllowed(bool allowed) { m_controlAllowed = allowed; }
    explicit Page5ViewModel(Page5Model *model, QObject *parent = nullptr);
    ~Page5ViewModel() override;

    // Borrowed provider; nullptr restores the model-backed fallback.
    void setConditionProvider(const ITestConditionProvider* provider) { m_conditionProvider = provider; }

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
    void validateXml(QXmlStreamReader& reader) const override;
    void loadXml(QXmlStreamReader& reader) override;

    Page5ExecutionContext executionContext() const;

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
    bool startExecution(const QVector<TaskPayload>& payloads);
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
    void taskStatusChanged(int runPanelIndex, TaskStatus status);
    void taskRetryCountChanged(int runPanelIndex, int attempt);
    void logMessage(const QString& msg);

private slots:
    void setRunning(bool running);

private:
    bool m_controlAllowed = true;
    Page5Model*                   m_model          = nullptr;
    const ITestConditionProvider* m_conditionProvider = nullptr;
    Oscilloscope*                 m_oscilloscope   = nullptr;
    Page5TestWorker* m_worker       = nullptr;
    QThread*         m_workerThread = nullptr;
    bool m_hasPage1Config                          = false;
    bool m_isRunning                               = false;
};
