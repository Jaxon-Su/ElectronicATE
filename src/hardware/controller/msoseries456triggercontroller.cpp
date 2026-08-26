#include "msoseries456triggercontroller.h"
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QDebug>
#include <QMessageBox>
#include "messageservice.h"
#include "smartstepspinbox.h"
#include <QSignalBlocker>
#include <QTimer>
#include <QStandardItem>
#include <QStandardItemModel>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
//  建構子：findChild 查找 UI 元件（objectName 與 DPO7000 Widget 相同）
// ─────────────────────────────────────────────────────────────────────────────
MSOSeries456TriggerController::MSOSeries456TriggerController(
    QWidget* triggerWidget, QObject* parent)
    : AbstractTriggerController(triggerWidget, parent)
{
    if (!triggerWidget) return;

    m_cmbTrigType    = triggerWidget->findChild<QComboBox*>("triggerType");
    m_cmbTrigSource  = triggerWidget->findChild<QComboBox*>("triggerSource");
    m_btnTrigRising  = triggerWidget->findChild<QPushButton*>("triggerRising");
    m_btnTrigFalling = triggerWidget->findChild<QPushButton*>("triggerFalling");
    m_btnTrigBoth    = triggerWidget->findChild<QPushButton*>("triggerBoth");
    m_btnTrigAuto    = triggerWidget->findChild<QPushButton*>("triggerAuto");
    m_btnTrigNorm    = triggerWidget->findChild<QPushButton*>("triggerNorm");
    m_btnTrigSingle  = triggerWidget->findChild<QPushButton*>("triggerSingle");
    m_btnTrigSet     = triggerWidget->findChild<QPushButton*>("triggerSet");
    m_btnRunstop     = triggerWidget->findChild<QPushButton*>("runStop");
    m_btnGetValue    = triggerWidget->findChild<QPushButton*>("getValue");
    m_spinTrigLevel  = triggerWidget->findChild<SmartStepSpinBox*>("triggerLevel");
    m_autoTrigScale  = triggerWidget->findChild<SmartStepSpinBox*>("triggerScale");
    m_autoTrigTarget = triggerWidget->findChild<SmartStepSpinBox*>("triggerTaget"); // 保留原拼寫
    m_lblTrigStatus  = triggerWidget->findChild<QLabel*>("triggerStatus");
    m_lblMeasMax     = triggerWidget->findChild<QLabel*>("measureMax");
    m_lblMeasMin     = triggerWidget->findChild<QLabel*>("measureMin");
    m_lblMeasRms     = triggerWidget->findChild<QLabel*>("measureRms");
    m_lblMeasMean    = triggerWidget->findChild<QLabel*>("measureMean");
    m_lblMeasAbsPeak = triggerWidget->findChild<QLabel*>("measureAbsPeak");
    m_btnTrigSteady  = triggerWidget->findChild<QPushButton*>("btntrig_Steady");

    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout,
            this, &MSOSeries456TriggerController::updateRunStopStatus);
    m_statusTimer->start(500);

    connectSignals();
}

MSOSeries456TriggerController::~MSOSeries456TriggerController()
{
    cleanup();
}

void MSOSeries456TriggerController::cleanup()
{
    cleanupWorkerThread();
    disconnect();

    if (m_statusTimer) {
        m_statusTimer->stop();
        delete m_statusTimer;
        m_statusTimer = nullptr;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  setInstrument（多型版本，從基類指標 dynamic_cast 到 MSOSeries456）
// ─────────────────────────────────────────────────────────────────────────────
void MSOSeries456TriggerController::setInstrument(Oscilloscope* instrument)
{
    m_reconnectCounter = 0;
    if (auto* mso = dynamic_cast<MSOSeries456*>(instrument)) {
        m_instrument = mso;
        updateTriggerStatus();
    } else if (instrument) {
        qWarning() << "[MSOSeries456TriggerController] 不相容的示波器型別："
                   << instrument->model();
        m_instrument = nullptr;
        updateTriggerStatus();
    } else {
        // nullptr：示波器被移除（reload config 期間），更新顯示狀態
        m_instrument = nullptr;
        updateTriggerStatus();
    }
}

void MSOSeries456TriggerController::setInstrument(MSOSeries456* instrument)
{
    m_instrument = instrument;
    updateTriggerStatus();
}

MSOSeries456* MSOSeries456TriggerController::getMSOInstrument() const
{
    return dynamic_cast<MSOSeries456*>(m_instrument);
}

// ─────────────────────────────────────────────────────────────────────────────
//  connectSignals：連接所有 UI 信號（結構與 DPO7000TriggerController 相同）
// ─────────────────────────────────────────────────────────────────────────────
void MSOSeries456TriggerController::connectSignals()
{
    if (m_btnTrigSingle)
        connect(m_btnTrigSingle, &QPushButton::clicked,
                this, &MSOSeries456TriggerController::onSingleTriggered);

    if (m_btnRunstop)
        connect(m_btnRunstop, &QPushButton::clicked,
                this, &MSOSeries456TriggerController::onRunStopTriggered);

    if (m_btnTrigAuto)
        connect(m_btnTrigAuto, &QPushButton::clicked,
                this, &MSOSeries456TriggerController::onAutoTriggered);

    if (m_btnTrigNorm)
        connect(m_btnTrigNorm, &QPushButton::clicked,
                this, &MSOSeries456TriggerController::onNormTriggered);

    if (m_btnTrigSet)
        connect(m_btnTrigSet, &QPushButton::clicked,
                this, &MSOSeries456TriggerController::onSetTriggered);

    if (m_cmbTrigType)
        connect(m_cmbTrigType, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &MSOSeries456TriggerController::onTriggerTypeChanged);

    if (m_cmbTrigSource)
        connect(m_cmbTrigSource, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &MSOSeries456TriggerController::onTriggerSourceChanged);

    if (m_btnTrigRising)
        connect(m_btnTrigRising, &QPushButton::clicked,
                this, &MSOSeries456TriggerController::onSlopeRisingTriggered);

    if (m_btnTrigFalling)
        connect(m_btnTrigFalling, &QPushButton::clicked,
                this, &MSOSeries456TriggerController::onSlopeFallingTriggered);

    if (m_btnTrigBoth)
        connect(m_btnTrigBoth, &QPushButton::clicked,
                this, &MSOSeries456TriggerController::onSlopeBothTriggered);

    if (m_btnGetValue)
        connect(m_btnGetValue, &QPushButton::clicked,
                this, &MSOSeries456TriggerController::onGetValueTriggered);

    if (m_btnTrigSteady) {
        // 按鈕文字隨 toggle 狀態更新（ON / OFF 顯示）
        connect(m_btnTrigSteady, &QPushButton::toggled,
                this, [this](bool on) {
                    m_btnTrigSteady->setText(
                        on ? tr("Semi-Auto Trigger ON")
                           : tr("Semi-Auto Trigger OFF"));
                });
        connect(m_btnTrigSteady, &QPushButton::toggled,
                this, &MSOSeries456TriggerController::onTriggerSteadyToggled);
    }

    if (m_autoTrigTarget)
        connect(m_autoTrigTarget,
                QOverload<double>::of(&SmartStepSpinBox::valueChanged),
                this, &MSOSeries456TriggerController::onTargetLevelChanged);

    if (m_autoTrigScale)
        connect(m_autoTrigScale,
                QOverload<double>::of(&SmartStepSpinBox::valueChanged),
                this, &MSOSeries456TriggerController::onStepScaleChanged);
}

// ═══ Slot 實作 ════════════════════════════════════════════════════════════════

void MSOSeries456TriggerController::onSingleTriggered()
{
    if (!checkInstrumentConnection()) { showConnectionError(); return; }

    qDebug() << "[MSOSeries456TriggerController] Single trigger activated";
    m_instrument->single();

    if (m_lblTrigStatus) {
        m_lblTrigStatus->setText("SINGLE");
        m_lblTrigStatus->setStyleSheet(
            "QLabel { "
            "background: #ffff7f; color: #000000; "
            "border-radius: 7px; padding: 2px 10px; font-weight: bold; "
            "}");
    }
    QTimer::singleShot(200, this, &MSOSeries456TriggerController::updateRunStopStatus);
}

void MSOSeries456TriggerController::onRunStopTriggered()
{
    if (!checkInstrumentConnection()) { showConnectionError(); return; }

    const bool running = m_instrument->isRunning();
    qDebug() << "[MSOSeries456TriggerController] Run/Stop - 目前狀態："
             << (running ? "Running" : "Stopped");

    if (running)
        m_instrument->stop();
    else
        m_instrument->run();

    QTimer::singleShot(100, this, &MSOSeries456TriggerController::updateRunStopStatus);
}

void MSOSeries456TriggerController::updateRunStopStatus()
{
    if (!m_instrument || !checkInstrumentConnection()) {
        updateTriggerStatus();
        if (++m_reconnectCounter >= kReconnectInterval) {
            m_reconnectCounter = 0;
            emit reconnectRequested();
        }
        return;
    }

    bool running = m_instrument->isRunning();

    if (!running && !m_instrument->lastError().isEmpty()) {
        m_instrument->disconnect();
        updateTriggerStatus();
        m_reconnectCounter = 0;
        emit reconnectRequested();
        return;
    }

    m_reconnectCounter = 0;
    setRunningUI(running);
}

void MSOSeries456TriggerController::setRunningUI(bool running)
{
    if (m_btnRunstop)
        m_btnRunstop->setText(running ? tr("Stop") : tr("Run"));

    if (m_lblTrigStatus) {
        if (running) {
            m_lblTrigStatus->setText("RUN");
            m_lblTrigStatus->setStyleSheet(
                "QLabel { background: #7fff7f; color: #000000; "
                "border-radius: 7px; padding: 2px 10px; font-weight: bold; }");
        } else {
            m_lblTrigStatus->setText("STOP");
            m_lblTrigStatus->setStyleSheet(
                "QLabel { background: #ff7f7f; color: #000000; "
                "border-radius: 7px; padding: 2px 10px; font-weight: bold; }");
        }
    }
}

void MSOSeries456TriggerController::onAutoTriggered()
{
    if (!checkInstrumentConnection()) { showConnectionError(); return; }
    qDebug() << "[MSOSeries456TriggerController] Auto mode";
    m_instrument->automode();
}

void MSOSeries456TriggerController::onNormTriggered()
{
    if (!checkInstrumentConnection()) { showConnectionError(); return; }
    qDebug() << "[MSOSeries456TriggerController] Normal mode";
    m_instrument->normal();
}

void MSOSeries456TriggerController::onSetTriggered()
{
    if (!checkInstrumentConnection()) { showConnectionError(); return; }
    if (!m_spinTrigLevel) return;

    const double level = m_spinTrigLevel->value();
    qDebug() << "[MSOSeries456TriggerController] Set trigger level:" << level;

    // MSO 4/5/6 的 setTriggerLevel() 內部會自動選取對應 CH<x>
    // 前提：setTriggerSource() 須先被呼叫以更新 m_triggerSource 快取
    m_instrument->setTriggerLevel(level);
}

void MSOSeries456TriggerController::onTriggerTypeChanged()
{
    if (!checkInstrumentConnection()) return;
    if (!m_cmbTrigType) return;

    const QString type = m_cmbTrigType->currentText();
    qDebug() << "[MSOSeries456TriggerController] Trigger type ->" << type;
    m_instrument->setTriggerType(type);
}

void MSOSeries456TriggerController::onTriggerSourceChanged()
{
    if (!checkInstrumentConnection()) return;
    if (!m_cmbTrigSource) return;

    const QString source = m_cmbTrigSource->currentText();
    qDebug() << "[MSOSeries456TriggerController] Trigger source ->" << source;

    // 先更新示波器端的 trigger source（同時更新驅動內的 m_triggerSource 快取），
    // 之後 setTriggerLevel() 才能正確選取對應的 CH<x> 電平暫存器
    m_instrument->setTriggerSource(source);
}

void MSOSeries456TriggerController::onSlopeRisingTriggered()
{
    if (!checkInstrumentConnection()) { showConnectionError(); return; }
    qDebug() << "[MSOSeries456TriggerController] Slope -> RISING";
    m_instrument->setTriggerSlope("RISING");
}

void MSOSeries456TriggerController::onSlopeFallingTriggered()
{
    if (!checkInstrumentConnection()) { showConnectionError(); return; }
    qDebug() << "[MSOSeries456TriggerController] Slope -> FALLING";
    m_instrument->setTriggerSlope("FALLING");
}

void MSOSeries456TriggerController::onSlopeBothTriggered()
{
    if (!checkInstrumentConnection()) { showConnectionError(); return; }
    qDebug() << "[MSOSeries456TriggerController] Slope -> BOTH";
    m_instrument->setTriggerSlope("BOTH");
}

void MSOSeries456TriggerController::onGetValueTriggered()
{
    if (!checkInstrumentConnection()) { showConnectionError(); return; }

    const int ch = getSelectedChannel();
    qDebug() << "[MSOSeries456TriggerController] Get value CH" << ch;

    if (m_btnGetValue) m_btnGetValue->setEnabled(false);

    const double maxVal  = m_instrument->measureSignalPeak(ch, "MAXimum");
    const double minVal  = m_instrument->measureSignalPeak(ch, "MINimum");
    const double rmsVal  = m_instrument->measureSignalPeak(ch, "RMS");
    const double meanVal = m_instrument->measureSignalPeak(ch, "MEAN");

    if (m_lblMeasMax)  m_lblMeasMax->setText(formatMeasureValue(maxVal));
    if (m_lblMeasMin)  m_lblMeasMin->setText(formatMeasureValue(minVal));
    if (m_lblMeasRms)  m_lblMeasRms->setText(formatMeasureValue(rmsVal));
    if (m_lblMeasMean) m_lblMeasMean->setText(formatMeasureValue(meanVal));

    const bool maxIsDominant = std::fabs(maxVal) >= std::fabs(minVal);
    const QString dominant = QString("%1 %2")
                                  .arg(maxIsDominant ? "Max" : "Min")
                                  .arg(formatMeasureValue(maxIsDominant ? maxVal : minVal));
    if (m_lblMeasAbsPeak) m_lblMeasAbsPeak->setText(dominant);

    if (m_lblTrigStatus) {
        m_lblTrigStatus->setText(QString("CH%1 MEASURED").arg(ch));
        m_lblTrigStatus->setStyleSheet(
            "QLabel { background: #7f7fff; color: #000000; "
            "border-radius: 7px; padding: 2px 10px; font-weight: bold; }");
    }

    if (m_btnGetValue) m_btnGetValue->setEnabled(true);
}

// ─────────────────────────────────────────────────────────────────────────────
//  半自動觸發（Semi-Auto Trigger）
// ─────────────────────────────────────────────────────────────────────────────
void MSOSeries456TriggerController::onTriggerSteadyToggled(bool on)
{
    qDebug() << QString("[MSOSeries456TriggerController] Semi-auto trigger: %1")
    .arg(on ? "ON" : "OFF");

    if (on) {
        // ── 前置檢查 ───────────────────────────────────────────────────────
        if (!checkInstrumentConnection()) {
            showConnectionError();
            goto reset_button;
        }

        if (!dynamic_cast<MSOSeries456*>(m_instrument)) {
            qWarning() << "[MSOSeries456TriggerController] 無效的示波器型別";
            MessageService::instance().showWarning("Error",
                                                   "Semi-auto trigger requires MSOSeries456 instrument");
            goto reset_button;
        }

        // ── 啟動 Worker Thread ─────────────────────────────────────────────
        try {
            setupWorkerThread();
            return; // 成功則不執行 reset_button
        } catch (const std::exception& e) {
            qCritical() << "[MSOSeries456TriggerController] setupWorkerThread 例外：" << e.what();
            MessageService::instance().showWarning("Semi-Auto Trigger Error",
                                                   QString("Failed to start: %1").arg(e.what()));
        } catch (...) {
            qCritical() << "[MSOSeries456TriggerController] setupWorkerThread 未知例外";
            MessageService::instance().showWarning("Semi-Auto Trigger Error",
                                                   "Unknown error occurred");
        }

        cleanupWorkerThread();

    reset_button:
    {
        QSignalBlocker blocker(m_btnTrigSteady);
        m_btnTrigSteady->setChecked(false);
        m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));
    }
        lockTriggerControls(false);

        if (m_lblTrigStatus) {
            m_lblTrigStatus->setText("ERROR");
            m_lblTrigStatus->setStyleSheet(
                "QLabel { background: #ff7f7f; border-radius:7px; padding:2px 10px; }");
        }

    } else {
        // ── 停止 ────────────────────────────────────────────────────────────
        qDebug() << "[MSOSeries456TriggerController] 停止半自動觸發";
        if (m_worker) m_worker->stopTracking();
        cleanupWorkerThread();
        lockTriggerControls(false);

        if (m_lblTrigStatus) {
            m_lblTrigStatus->setText("STOPPED");
            m_lblTrigStatus->setStyleSheet(
                "QLabel { background: #ffff7f; border-radius:7px; padding:2px 10px; }");
        }
    }
}

void MSOSeries456TriggerController::onTargetLevelChanged()
{
    if (m_worker && m_autoTrigTarget) {
        const double targetLevel = m_autoTrigTarget->value();
        QMetaObject::invokeMethod(m_worker, "setTargetLevel",
                                  Qt::QueuedConnection,
                                  Q_ARG(double, targetLevel));
        qDebug() << QString("[MSOSeries456TriggerController] Target level -> %1V")
                        .arg(targetLevel, 0, 'f', 3);
    }
}

void MSOSeries456TriggerController::onStepScaleChanged()
{
    if (m_worker && m_autoTrigScale) {
        const double stepScale = m_autoTrigScale->value();
        QMetaObject::invokeMethod(m_worker, "setStepScale",
                                  Qt::QueuedConnection,
                                  Q_ARG(double, stepScale));
        qDebug() << QString("[MSOSeries456TriggerController] Step scale -> %1V")
                        .arg(stepScale, 0, 'f', 3);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  setupWorkerThread：建立 AutoTriggerWorker 並啟動執行緒
//
//  ★ 重要提醒：AutoTriggerWorker 目前可能只接受 DPO7000* 建構。
//    若如此，有兩種修改方式（擇一）：
//
//    方案 A（推薦）：將 AutoTriggerWorker 建構子改為
//      explicit AutoTriggerWorker(Oscilloscope* osc, QObject* parent = nullptr)
//      — 所有用到的方法（getTriggerLevel / setTriggerLevel / measureSignalPeak）
//        在 Oscilloscope 基類已有虛擬介面，MSOSeries456 均已實作。
//
//    方案 B：複製一份 MSOAutoTriggerWorker，將 DPO7000* 替換為 MSOSeries456*。
//
//  本檔案以方案 A 為前提撰寫（傳入 Oscilloscope* 基類指標）。
// ─────────────────────────────────────────────────────────────────────────────
void MSOSeries456TriggerController::setupWorkerThread()
{
    if (!checkInstrumentConnection()) {
        showConnectionError();
        QSignalBlocker blocker(m_btnTrigSteady);
        m_btnTrigSteady->setChecked(false);
        m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));
        return;
    }

    if (!m_autoTrigTarget || !m_autoTrigScale || !m_spinTrigLevel) {
        qWarning() << "[MSOSeries456TriggerController] 必要的 SpinBox 未找到";
        QSignalBlocker blocker(m_btnTrigSteady);
        m_btnTrigSteady->setChecked(false);
        m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));
        lockTriggerControls(false);
        return;
    }

    if (m_worker || m_workerThread) {
        qWarning() << "[MSOSeries456TriggerController] Worker 已存在，先清理";
        cleanupWorkerThread();
    }

    MSOSeries456* mso = dynamic_cast<MSOSeries456*>(m_instrument);
    if (!mso) {
        showConnectionError();
        QSignalBlocker blocker(m_btnTrigSteady);
        m_btnTrigSteady->setChecked(false);
        m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));
        lockTriggerControls(false);
        return;
    }

    m_workerThread = new QThread(this);

    // AutoTriggerWorker 建議以 Oscilloscope* 接受（方案 A），
    // 若仍為 DPO7000* 版本，請先更新其建構子。
    m_worker = new AutoTriggerWorker(mso, nullptr);
    m_worker->moveToThread(m_workerThread);

    m_worker->setStartLevel(m_spinTrigLevel->value());
    m_worker->setTargetLevel(m_autoTrigTarget->value());
    m_worker->setStepScale(m_autoTrigScale->value());

    // ── 連接 Worker 信號 ─────────────────────────────────────────────────
    connect(m_workerThread, &QThread::started,
            m_worker, &AutoTriggerWorker::startTracking);

    connect(m_worker, &AutoTriggerWorker::targetReached,
            this, [this](double finalLevel) {
                qDebug() << QString("[MSOSeries456TriggerController] 達到目標電平: %1V")
                                .arg(finalLevel, 0, 'f', 3);
                if (m_spinTrigLevel) {
                    QSignalBlocker blocker(m_spinTrigLevel);
                    m_spinTrigLevel->setValue(finalLevel);
                }
                if (m_lblTrigStatus) {
                    m_lblTrigStatus->setText("TARGET REACHED");
                    m_lblTrigStatus->setStyleSheet(
                        "QLabel { background: #7fff7f; border-radius:7px; padding:2px 10px; }");
                }
            });

    connect(m_worker, &AutoTriggerWorker::adjustmentProgress,
            this, [this](double currentLevel, int stepCount) {
                if (m_spinTrigLevel) {
                    QSignalBlocker blocker(m_spinTrigLevel);
                    m_spinTrigLevel->setValue(currentLevel);
                }
                if (m_lblTrigStatus) {
                    m_lblTrigStatus->setText(
                        QString("STEP %1: %2V").arg(stepCount).arg(currentLevel, 0, 'f', 1));
                    m_lblTrigStatus->setStyleSheet(
                        "QLabel { background: #7f7fff; border-radius:7px; padding:2px 10px; }");
                }
            });

    connect(m_worker, &AutoTriggerWorker::trackingCompleted,
            this, [this](bool success, const QString& message) {
                qDebug() << QString("[MSOSeries456TriggerController] 半自動觸發結束: %1 - %2")
                                .arg(success ? "成功" : "失敗").arg(message);

                QSignalBlocker blocker(m_btnTrigSteady);
                m_btnTrigSteady->setChecked(false);
                m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));

                cleanupWorkerThread();
                lockTriggerControls(false);

                if (m_lblTrigStatus) {
                    m_lblTrigStatus->setText(success ? "COMPLETED" : "FAILED");
                    m_lblTrigStatus->setStyleSheet(
                        success
                            ? "QLabel { background: #7fff7f; border-radius:7px; padding:2px 10px; }"
                            : "QLabel { background: #ff7f7f; border-radius:7px; padding:2px 10px; }");
                }

                MessageService::instance().showInfo("Semi-Auto Trigger", message);
            });

    connect(m_worker, &AutoTriggerWorker::trackingError,
            this, [this](const QString& error) {
                qWarning() << "[MSOSeries456TriggerController] 半自動觸發錯誤：" << error;

                QSignalBlocker blocker(m_btnTrigSteady);
                m_btnTrigSteady->setChecked(false);
                m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));

                cleanupWorkerThread();
                lockTriggerControls(false);

                if (m_lblTrigStatus) {
                    m_lblTrigStatus->setText("ERROR");
                    m_lblTrigStatus->setStyleSheet(
                        "QLabel { background: #ff7f7f; border-radius:7px; padding:2px 10px; }");
                }

                MessageService::instance().showWarning("Semi-Auto Trigger Error", error);
            });

    m_workerThread->start();
    lockTriggerControls(true);

    if (m_lblTrigStatus) {
        m_lblTrigStatus->setText("STARTING");
        m_lblTrigStatus->setStyleSheet(
            "QLabel { background: #7f7fff; border-radius:7px; padding:2px 10px; }");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  cleanupWorkerThread：確保 worker thread 完全停止後再釋放記憶體
//  （避免 DPO7000TriggerController 原版只設 nullptr 而洩漏的問題）
// ─────────────────────────────────────────────────────────────────────────────
void MSOSeries456TriggerController::cleanupWorkerThread()
{
    if (m_workerThread) {
        if (m_workerThread->isRunning()) {
            m_workerThread->quit();
            if (!m_workerThread->wait(5000)) {
                m_workerThread->terminate();
                m_workerThread->wait();
            }
        }
        if (m_worker) {
            delete m_worker;
            m_worker = nullptr;
        }
        delete m_workerThread;
        m_workerThread = nullptr;
    } else {
        if (m_worker) {
            delete m_worker;
            m_worker = nullptr;
        }
    }
}

// ═══ 狀態更新 ════════════════════════════════════════════════════════════════

void MSOSeries456TriggerController::updateTriggerStatus()
{
    if (!m_lblTrigStatus) return;

    if (m_instrument && m_instrument->isConnected()) {
        m_lblTrigStatus->setText("READY");
        m_lblTrigStatus->setStyleSheet(
            "QLabel { background: #7fff7f; border-radius:7px; padding:2px 10px; }");
    } else {
        m_lblTrigStatus->setText("DISCONNECTED");
        m_lblTrigStatus->setStyleSheet(
            "QLabel { background: #ff7f7f; border-radius:7px; padding:2px 10px; }");
    }
}

// ═══ 共用輔助 ════════════════════════════════════════════════════════════════

bool MSOSeries456TriggerController::checkInstrumentConnection() const
{
    return m_instrument && m_instrument->isConnected();
}

void MSOSeries456TriggerController::showConnectionError() const
{
    MessageService::instance().showWarning(
        "Error Message",
        "[MSOSeries456TriggerController] Instrument not connected");
    qWarning() << "[MSOSeries456TriggerController] 示波器未連線";
}

void MSOSeries456TriggerController::lockTriggerControls(bool lock)
{
    for (QWidget* w : getTriggerControlWidgets())
        if (w) w->setEnabled(!lock);
}

QList<QWidget*> MSOSeries456TriggerController::getTriggerControlWidgets() const
{
    QList<QWidget*> widgets;
    if (m_cmbTrigType)    widgets << m_cmbTrigType;
    if (m_cmbTrigSource)  widgets << m_cmbTrigSource;
    if (m_btnTrigRising)  widgets << m_btnTrigRising;
    if (m_btnTrigFalling) widgets << m_btnTrigFalling;
    if (m_btnTrigBoth)    widgets << m_btnTrigBoth;
    if (m_btnTrigAuto)    widgets << m_btnTrigAuto;
    if (m_btnTrigNorm)    widgets << m_btnTrigNorm;
    if (m_btnTrigSingle)  widgets << m_btnTrigSingle;
    if (m_btnTrigSet)     widgets << m_btnTrigSet;
    if (m_btnRunstop)     widgets << m_btnRunstop;
    if (m_btnGetValue)    widgets << m_btnGetValue;
    if (m_spinTrigLevel)  widgets << m_spinTrigLevel;
    return widgets;
}

QString MSOSeries456TriggerController::formatMeasureValue(double value) const
{
    if (!std::isfinite(value))
        return "N/A";

    return QString("%1 V").arg(value, 0, 'g', 6);
}

QList<int> MSOSeries456TriggerController::getCheckedChannels() const
{
    QList<int> channels;
    if (!m_cmbTrigSource) return channels;

    auto* model = qobject_cast<QStandardItemModel*>(m_cmbTrigSource->model());
    if (!model) return channels;

    for (int i = 0; i < model->rowCount(); ++i) {
        QStandardItem* item = model->item(i);
        if (item && item->checkState() == Qt::Checked)
            channels.append(i + 1);
    }
    return channels;
}

QString MSOSeries456TriggerController::getCurrentTriggerSource() const
{
    return m_cmbTrigSource ? m_cmbTrigSource->currentText() : "CH1";
}
