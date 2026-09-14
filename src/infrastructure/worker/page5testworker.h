#pragma once
#include <QObject>
#include <QVector>
#include <QAtomicInt>
#include "page5taskpayload.h"
#include "taskstatus.h"

class IOscilloscopeMeasureStrategy;

// ══════════════════════════════════════════════════════
//  Page5TestWorker — 非同步執行層
//
//  執行緒模型：
//   物件由 Page5ViewModel 建立並 moveToThread(m_workerThread)
//   startTasks(payloads, context) 透過 queued lambda 接收不可變的執行資料副本
//   UI 更新透過 signal 回到主執行緒（自動 queued connection）
//
//  執行流程：
//   ViewModel::startExecution() → startTasks(payloads)  [worker thread]
//     → 逐列 executeTask()
//       → emit taskStatusChanged()  [→ ViewModel → View RunPanel]
//       → emit logMessage()         [→ ViewModel → View OutputWindow]
//     → emit finished()             [→ ViewModel::setRunning(false)]
//
//  中止：
//   stop() 設 m_stopRequested=1
//   startTasks() 每列執行前檢查；通訊中的命令返回後依停止檢查結束
// ══════════════════════════════════════════════════════
class Page5TestWorker : public QObject
{
    Q_OBJECT

public:
    explicit Page5TestWorker(QObject* parent = nullptr);
    void prepareRun() { m_stopRequested.storeRelease(0); }
    ~Page5TestWorker() override = default;


public slots:
    void startTasks(const QVector<TaskPayload>& payloads, const Page5ExecutionContext& context);
    void stop();

signals:
    void taskStatusChanged(int runPanelIndex, TaskStatus status);
    void retryCountChanged(int runPanelIndex, int attempt);
    void logMessage(const QString& msg);
    void finished();

private:
    bool executeTask(int runPanelIndex, const TaskPayload& payload);

    bool executeDelay           (const QVariantMap& cfg);
    bool executeWriteOscilloscope(const QVariantMap& cfg);
    bool executeTurnOn          (const QVariantMap& cfg);
    bool executeTurnOff         (const QVariantMap& cfg);
    bool executeRelay           (const QVariantMap& cfg);
    bool executeStaticTest      (const QVariantMap& cfg);
    bool executeDynamicTest     (const QVariantMap& cfg);
    bool executeTurnOnThenShort (const QVariantMap& cfg);
    bool executeShortThenTurnOn (const QVariantMap& cfg);

    Page5ExecutionContext m_context;
    QAtomicInt      m_stopRequested = 0;
};
