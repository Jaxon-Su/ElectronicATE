#include "DPO7000TriggerController.h"
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QDebug>
#include <QMessageBox>
#include "messageservice.h"
#include "smartstepspinbox.h"
#include <QSignalBlocker>
#include <QTimer>

DPO7000TriggerController::DPO7000TriggerController(QWidget* triggerWidget, QObject* parent)
    : AbstractTriggerController(triggerWidget, parent)
{
    if (!triggerWidget) return;

    m_cmbTrigType = triggerWidget->findChild<QComboBox*>("triggerType");
    m_cmbTrigSource = triggerWidget->findChild<QComboBox*>("triggerSource");
    m_btnTrigRising = triggerWidget->findChild<QPushButton*>("triggerRising");
    m_btnTrigFalling = triggerWidget->findChild<QPushButton*>("triggerFalling");
    m_btnTrigBoth = triggerWidget->findChild<QPushButton*>("triggerBoth");
    m_btnTrigAuto = triggerWidget->findChild<QPushButton*>("triggerAuto");
    m_btnTrigNorm = triggerWidget->findChild<QPushButton*>("triggerNorm");
    m_btnTrigSingle = triggerWidget->findChild<QPushButton*>("triggerSingle");
    m_btnTrigSet = triggerWidget->findChild<QPushButton*>("triggerSet");
    m_btnRunstop = triggerWidget->findChild<QPushButton*>("runStop");
    m_spinTrigLevel = triggerWidget->findChild<SmartStepSpinBox*>("triggerLevel");
    m_autoTrigScale = triggerWidget->findChild<SmartStepSpinBox*>("triggerScale");
    m_autoTrigTarget = triggerWidget->findChild<SmartStepSpinBox*>("triggerTaget");
    m_lblTrigStatus = triggerWidget->findChild<QLabel*>("triggerStatus");
    m_btnTrigSteady = triggerWidget->findChild<QPushButton*>("btntrig_Steady");

    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout,
            this, &DPO7000TriggerController::updateRunStopStatus);
    m_statusTimer->start(500);

    connectSignals();
}

DPO7000TriggerController::~DPO7000TriggerController()
{
    cleanup();
}

void DPO7000TriggerController::cleanup()
{
    // 清理 worker thread
    cleanupWorkerThread();

    // 斷開所有信號
    disconnect();

    // 清理其他資源
    if (m_statusTimer) {
        m_statusTimer->stop();
        delete m_statusTimer;
        m_statusTimer = nullptr;
    }
}

void DPO7000TriggerController::setInstrument(Oscilloscope* instrument)
{
    if (auto* dpo7000 = dynamic_cast<DPO7000*>(instrument)) {
        m_instrument = dpo7000;
        updateTriggerStatus();
    } else if (instrument) {
        qWarning() << "[DPO7000TriggerController] Incompatible instrument type:"
                   << instrument->model();
        m_instrument = nullptr;
    } else {
        m_instrument = nullptr;
    }
}

void DPO7000TriggerController::setInstrument(DPO7000* instrument)
{
    m_instrument = instrument;
    updateTriggerStatus();
}

DPO7000* DPO7000TriggerController::getDPO7000Instrument() const
{
    return dynamic_cast<DPO7000*>(m_instrument);
}

void DPO7000TriggerController::connectSignals()
{
    if (m_btnTrigSingle) {
        connect(m_btnTrigSingle, &QPushButton::clicked,
                this, &DPO7000TriggerController::onSingleTriggered);
    }

    if (m_btnRunstop) {
        connect(m_btnRunstop, &QPushButton::clicked,
                this, &DPO7000TriggerController::onRunStopTriggered);
    }

    if (m_btnTrigAuto) {
        connect(m_btnTrigAuto, &QPushButton::clicked,
                this, &DPO7000TriggerController::onAutoTriggered);
    }

    if (m_btnTrigNorm) {
        connect(m_btnTrigNorm, &QPushButton::clicked,
                this, &DPO7000TriggerController::onNormTriggered);
    }

    if (m_btnTrigSet) {
        connect(m_btnTrigSet, &QPushButton::clicked,
                this, &DPO7000TriggerController::onSetTriggered);
    }

    if (m_cmbTrigType) {
        connect(m_cmbTrigType, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &DPO7000TriggerController::onTriggerTypeChanged);
    }

    if (m_cmbTrigSource) {
        connect(m_cmbTrigSource, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &DPO7000TriggerController::onTriggerSourceChanged);
    }

    if (m_btnTrigRising) {
        connect(m_btnTrigRising, &QPushButton::clicked,
                this, &DPO7000TriggerController::onSlopeRisingTriggered);
    }

    if (m_btnTrigFalling) {
        connect(m_btnTrigFalling, &QPushButton::clicked,
                this, &DPO7000TriggerController::onSlopeFallingTriggered);
    }

    if (m_btnTrigBoth) {
        connect(m_btnTrigBoth, &QPushButton::clicked,
                this, &DPO7000TriggerController::onSlopeBothTriggered);
    }

    if (m_btnTrigSteady) {
        connect(m_btnTrigSteady, &QPushButton::toggled,
                this, [this](bool on) {
                    m_btnTrigSteady->setText(on ? tr("Semi-Auto Trigger ON") : tr("Semi-Auto Trigger OFF"));
                });

        connect(m_btnTrigSteady, &QPushButton::toggled,
                this, &DPO7000TriggerController::onTriggerSteadyToggled);
    }

    if (m_autoTrigTarget) {
        connect(m_autoTrigTarget, QOverload<double>::of(&SmartStepSpinBox::valueChanged),
                this, &DPO7000TriggerController::onTargetLevelChanged);
    }

    if (m_autoTrigScale) {
        connect(m_autoTrigScale, QOverload<double>::of(&SmartStepSpinBox::valueChanged),
                this, &DPO7000TriggerController::onStepScaleChanged);
    }
}

void DPO7000TriggerController::onSingleTriggered()
{
    if (!checkInstrumentConnection()) {
        showConnectionError();
        return;
    }

    qDebug() << "[DPO7000TriggerController] Single trigger activated";
    m_instrument->single();

    if (m_lblTrigStatus) {
        m_lblTrigStatus->setText("SINGLE");
        m_lblTrigStatus->setStyleSheet(
            "QLabel { "
            "background: #ffff7f; "
            "color: #000000; "
            "border-radius: 7px; "
            "padding: 2px 10px; "
            "font-weight: bold; "
            "}"
            );
    }

    QTimer::singleShot(200, this, &DPO7000TriggerController::updateRunStopStatus);
}

void DPO7000TriggerController::onRunStopTriggered()
{
    if (!checkInstrumentConnection()) {
        showConnectionError();
        return;
    }

    bool isRunning = m_instrument->isRunning();

    qDebug() << "[DPO7000TriggerController] Current state:"
             << (isRunning ? "Running" : "Stopped");

    if (isRunning) {
        qDebug() << "[DPO7000TriggerController] Stopping acquisition...";
        m_instrument->stop();
    } else {
        qDebug() << "[DPO7000TriggerController] Starting acquisition...";
        m_instrument->run();
    }

    QTimer::singleShot(100, this, &DPO7000TriggerController::updateRunStopStatus);
}

void DPO7000TriggerController::updateRunStopStatus()
{
    if (!m_instrument || !checkInstrumentConnection()) {
        return;
    }

    bool isRunning = m_instrument->isRunning();
    setRunningUI(isRunning);
}

void DPO7000TriggerController::setRunningUI(bool running)
{
    if (m_btnRunstop) {
        m_btnRunstop->setText(running ? tr("Stop") : tr("Run"));
    }

    if (m_lblTrigStatus) {
        if (running) {
            m_lblTrigStatus->setText("RUN");
            m_lblTrigStatus->setStyleSheet(
                "QLabel { "
                "background: #7fff7f; "
                "color: #000000; "
                "border-radius: 7px; "
                "padding: 2px 10px; "
                "font-weight: bold; "
                "}"
                );
        } else {
            m_lblTrigStatus->setText("STOP");
            m_lblTrigStatus->setStyleSheet(
                "QLabel { "
                "background: #ff7f7f; "
                "color: #000000; "
                "border-radius: 7px; "
                "padding: 2px 10px; "
                "font-weight: bold; "
                "}"
                );
        }
    }
}

void DPO7000TriggerController::onAutoTriggered()
{
    if (!checkInstrumentConnection()) {
        showConnectionError();
        return;
    }
    qDebug() << "[DPO7000TriggerController] Auto setup triggered";
    m_instrument->automode();
}

void DPO7000TriggerController::onNormTriggered()
{
    if (!checkInstrumentConnection()) {
        showConnectionError();
        return;
    }
    qDebug() << "[DPO7000TriggerController] Normal mode triggered";
    m_instrument->normal();
}

void DPO7000TriggerController::onSetTriggered()
{
    if (!checkInstrumentConnection()) {
        showConnectionError();
        return;
    }
    double level = m_spinTrigLevel->value();
    qDebug() << "[DPO7000TriggerController] Trigger level set to:" << level;
    m_instrument->setTriggerLevel(level);
}

void DPO7000TriggerController::onTriggerTypeChanged()
{
    if (!checkInstrumentConnection()) {
        showConnectionError();
        return;
    }

    QString type = m_cmbTrigType->currentText();
    qDebug() << "[DPO7000TriggerController] Trigger type changed to:" << type;
    m_instrument->setTriggerType(type);
}

void DPO7000TriggerController::onTriggerSourceChanged()
{
    if (!checkInstrumentConnection()) {
        showConnectionError();
        return;
    }

    QString source = m_cmbTrigSource->currentText();
    qDebug() << "[DPO7000TriggerController] Trigger source changed to:" << source;
    m_instrument->setTriggerSource(source);
}

void DPO7000TriggerController::onSlopeRisingTriggered()
{
    if (!checkInstrumentConnection()) {
        showConnectionError();
        return;
    }
    qDebug() << "[DPO7000TriggerController] Trigger slope set to RISING";
    m_instrument->setTriggerSlope("RISING");
}

void DPO7000TriggerController::onSlopeFallingTriggered()
{
    if (!checkInstrumentConnection()) {
        showConnectionError();
        return;
    }
    qDebug() << "[DPO7000TriggerController] Trigger slope set to FALLING";
    m_instrument->setTriggerSlope("FALLING");
}

void DPO7000TriggerController::onSlopeBothTriggered()
{
    if (!checkInstrumentConnection()) {
        showConnectionError();
        return;
    }
    qDebug() << "[DPO7000TriggerController] Trigger slope set to BOTH";
    m_instrument->setTriggerSlope("BOTH");
}

void DPO7000TriggerController::onTriggerSteadyToggled(bool on)
{
    qDebug() << QString("[DPO7000TriggerController] Semi-auto trigger toggled: %1").arg(on ? "ON" : "OFF");

    if (on) {
        // ====== 第一步：檢查儀器連接 ======
        if (!checkInstrumentConnection()) {
            showConnectionError();

            // 恢復按鈕狀態
            QSignalBlocker blocker(m_btnTrigSteady);
            m_btnTrigSteady->setChecked(false);
            m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));

            // 解鎖所有控制項
            lockTriggerControls(false);

            // 更新狀態顯示
            if (m_lblTrigStatus) {
                m_lblTrigStatus->setText("ERROR: NO CONNECTION");
                m_lblTrigStatus->setStyleSheet(
                    "QLabel { background: #ff7f7f; border-radius:7px; padding:2px 10px; }");
            }
            return;
        }

        // ====== 第二步：檢查儀器類型 ======
        DPO7000* dpo7000 = dynamic_cast<DPO7000*>(m_instrument);
        if (!dpo7000) {
            qWarning() << "[DPO7000TriggerController] Invalid instrument type for semi-auto trigger";
            MessageService::instance().showWarning("Error",
                                                   "Semi-auto trigger requires DPO7000 instrument");

            // 恢復按鈕狀態
            QSignalBlocker blocker(m_btnTrigSteady);
            m_btnTrigSteady->setChecked(false);
            m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));

            // 解鎖所有控制項
            lockTriggerControls(false);

            // 更新狀態顯示
            if (m_lblTrigStatus) {
                m_lblTrigStatus->setText("ERROR: INVALID INSTRUMENT");
                m_lblTrigStatus->setStyleSheet(
                    "QLabel { background: #ff7f7f; border-radius:7px; padding:2px 10px; }");
            }
            return;
        }

        // ====== 第三步：使用 try-catch 保護啟動 Worker ======
        // 這是關鍵修復點！
        try {
            setupWorkerThread();  // 啟動 Semi-Auto Trigger
        } catch (const std::exception& e) {
            qCritical() << "[DPO7000TriggerController] Exception in setupWorkerThread:" << e.what();

            // 清理並恢復
            cleanupWorkerThread();
            QSignalBlocker blocker(m_btnTrigSteady);
            m_btnTrigSteady->setChecked(false);
            m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));
            lockTriggerControls(false);  // ✅ 確保解鎖

            if (m_lblTrigStatus) {
                m_lblTrigStatus->setText("ERROR");
                m_lblTrigStatus->setStyleSheet(
                    "QLabel { background: #ff7f7f; border-radius:7px; padding:2px 10px; }");
            }

            MessageService::instance().showWarning("Semi-Auto Trigger Error",
                                                   QString("Failed to start: %1").arg(e.what()));
        } catch (...) {
            qCritical() << "[DPO7000TriggerController] Unknown exception in setupWorkerThread";

            // 清理並恢復
            cleanupWorkerThread();
            QSignalBlocker blocker(m_btnTrigSteady);
            m_btnTrigSteady->setChecked(false);
            m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));
            lockTriggerControls(false);  // ✅ 確保解鎖

            if (m_lblTrigStatus) {
                m_lblTrigStatus->setText("ERROR");
                m_lblTrigStatus->setStyleSheet(
                    "QLabel { background: #ff7f7f; border-radius:7px; padding:2px 10px; }");
            }

            MessageService::instance().showWarning("Semi-Auto Trigger Error",
                                                   "Unknown error occurred");
        }
    } else {
        // ====== 關閉 Semi-Auto Trigger ======
        qDebug() << "[DPO7000TriggerController] Stopping semi-auto trigger";

        if (m_worker) {
            m_worker->stopTracking();
        }

        cleanupWorkerThread();
        lockTriggerControls(false);

        if (m_lblTrigStatus) {
            m_lblTrigStatus->setText("STOPPED");
            m_lblTrigStatus->setStyleSheet(
                "QLabel { background: #ffff7f; border-radius:7px; padding:2px 10px; }");
        }
    }
}

void DPO7000TriggerController::onTargetLevelChanged()
{
    if (m_worker && m_autoTrigTarget) {
        double targetLevel = m_autoTrigTarget->value();
        QMetaObject::invokeMethod(m_worker, "setTargetLevel",
                                  Qt::QueuedConnection,
                                  Q_ARG(double, targetLevel));
        qDebug() << QString("[DPO7000TriggerController] Target level updated to: %1V")
                        .arg(targetLevel, 0, 'f', 3);
    }
}

void DPO7000TriggerController::onStepScaleChanged()
{
    if (m_worker && m_autoTrigScale) {
        double stepScale = m_autoTrigScale->value();
        QMetaObject::invokeMethod(m_worker, "setStepScale",
                                  Qt::QueuedConnection,
                                  Q_ARG(double, stepScale));
        qDebug() << QString("[DPO7000TriggerController] Step scale updated to: %1V")
                        .arg(stepScale, 0, 'f', 3);
    }
}

void DPO7000TriggerController::setupWorkerThread()
{
    // 檢查儀器連接
    if (!checkInstrumentConnection()) {
        showConnectionError();
        QSignalBlocker blocker(m_btnTrigSteady);
        m_btnTrigSteady->setChecked(false);
        m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));
        return;
    }

    // 驗證必要的 UI 元件
    if (!m_autoTrigTarget || !m_autoTrigScale || !m_spinTrigLevel) {
        qWarning() << "[DPO7000TriggerController] Required spinboxes not found";
        QSignalBlocker blocker(m_btnTrigSteady);
        m_btnTrigSteady->setChecked(false);
        m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));
        lockTriggerControls(false);
        return;
    }

    // 防止重複創建 - 先清理舊的資源
    if (m_worker || m_workerThread) {
        qWarning() << "[DPO7000TriggerController] Worker already exists, cleaning up first";
        cleanupWorkerThread();
    }

    // 驗證儀器類型
    DPO7000* dpo7000 = dynamic_cast<DPO7000*>(m_instrument);
    if (!dpo7000) {
        showConnectionError();
        QSignalBlocker blocker(m_btnTrigSteady);
        m_btnTrigSteady->setChecked(false);
        m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));
        lockTriggerControls(false);
        return;
    }

    // 創建新的 worker 和 thread

    m_workerThread = new QThread(this);  // Controller 是父對象
    m_worker = new AutoTriggerWorker(dpo7000, nullptr);  // 暫時沒有父對象
    m_worker->moveToThread(m_workerThread);

    // 設置參數
    double startLevel = m_spinTrigLevel->value();
    double targetLevel = m_autoTrigTarget->value();
    double stepScale = m_autoTrigScale->value();

    m_worker->setStartLevel(startLevel);
    m_worker->setTargetLevel(targetLevel);
    m_worker->setStepScale(stepScale);

    // 連接信號
    connect(m_workerThread, &QThread::started,
            m_worker, &AutoTriggerWorker::startTracking);

    connect(m_worker, &AutoTriggerWorker::targetReached,
            this, [this](double finalLevel) {
                qDebug() << QString("[DPO7000TriggerController] Target reached at: %1V")
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
                    m_lblTrigStatus->setText(QString("STEP %1: %2V")
                                                 .arg(stepCount)
                                                 .arg(currentLevel, 0, 'f', 1));
                    m_lblTrigStatus->setStyleSheet(
                        "QLabel { background: #7f7fff; border-radius:7px; padding:2px 10px; }");
                }
            });

    connect(m_worker, &AutoTriggerWorker::trackingCompleted,
            this, [this](bool success, const QString& message) {
                qDebug() << QString("[DPO7000TriggerController] Tracking completed: %1 - %2")
                .arg(success ? "Success" : "Failed").arg(message);

                // 自動關閉按鈕
                QSignalBlocker blocker(m_btnTrigSteady);
                m_btnTrigSteady->setChecked(false);
                m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));

                // 清理資源
                cleanupWorkerThread();
                lockTriggerControls(false);

                if (m_lblTrigStatus) {
                    if (success) {
                        m_lblTrigStatus->setText("COMPLETED");
                        m_lblTrigStatus->setStyleSheet(
                            "QLabel { background: #7fff7f; border-radius:7px; padding:2px 10px; }");
                    } else {
                        m_lblTrigStatus->setText("FAILED");
                        m_lblTrigStatus->setStyleSheet(
                            "QLabel { background: #ff7f7f; border-radius:7px; padding:2px 10px; }");
                    }
                }

                MessageService::instance().showInfo("Semi-Auto Trigger", message);
            });

    connect(m_worker, &AutoTriggerWorker::trackingError,
            this, [this](const QString& error) {
                qWarning() << "[DPO7000TriggerController] Semi-auto trigger error:" << error;

                // 自動關閉按鈕
                QSignalBlocker blocker(m_btnTrigSteady);
                m_btnTrigSteady->setChecked(false);
                m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));

                // 清理資源
                cleanupWorkerThread();
                lockTriggerControls(false);

                if (m_lblTrigStatus) {
                    m_lblTrigStatus->setText("ERROR");
                    m_lblTrigStatus->setStyleSheet(
                        "QLabel { background: #ff7f7f; border-radius:7px; padding:2px 10px; }");
                }

                MessageService::instance().showWarning("Semi-Auto Trigger Error", error);
            });

    // 啟動 thread
    m_workerThread->start();

    // 鎖定其他控制項
    lockTriggerControls(true);

    // 更新狀態
    if (m_lblTrigStatus) {
        m_lblTrigStatus->setText("STARTING");
        m_lblTrigStatus->setStyleSheet(
            "QLabel { background: #7f7fff; border-radius:7px; padding:2px 10px; }");
    }
}

void DPO7000TriggerController::cleanupWorkerThread()
{
    if (m_workerThread) {
        if (m_workerThread->isRunning()) {
            m_workerThread->quit();
            if (!m_workerThread->wait(5000)) {
                m_workerThread->terminate();
                m_workerThread->wait();
            }
        }
        delete m_workerThread;
        m_workerThread = nullptr;
    }

    // m_worker 會被 thread 自動刪除（如果設定了父子關係）
    m_worker = nullptr;
}

void DPO7000TriggerController::lockTriggerControls(bool lock)
{
    QList<QWidget*> controlWidgets = getTriggerControlWidgets();

    for (QWidget* widget : controlWidgets) {
        if (widget) {
            widget->setEnabled(!lock);
        }
    }
}

QList<QWidget*> DPO7000TriggerController::getTriggerControlWidgets() const
{
    QList<QWidget*> widgets;

    if (m_cmbTrigType) widgets << m_cmbTrigType;
    if (m_cmbTrigSource) widgets << m_cmbTrigSource;
    if (m_btnTrigRising) widgets << m_btnTrigRising;
    if (m_btnTrigFalling) widgets << m_btnTrigFalling;
    if (m_btnTrigBoth) widgets << m_btnTrigBoth;
    if (m_btnTrigAuto) widgets << m_btnTrigAuto;
    if (m_btnTrigNorm) widgets << m_btnTrigNorm;
    if (m_btnTrigSingle) widgets << m_btnTrigSingle;
    if (m_btnTrigSet) widgets << m_btnTrigSet;
    if (m_btnRunstop) widgets << m_btnRunstop;
    if (m_spinTrigLevel) widgets << m_spinTrigLevel;

    return widgets;
}

void DPO7000TriggerController::updateTriggerStatus()
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

bool DPO7000TriggerController::checkInstrumentConnection() const
{
    return m_instrument && m_instrument->isConnected();
}

void DPO7000TriggerController::showConnectionError() const
{
    MessageService::instance().showWarning("Error Message",
                                           "[DPO7000TriggerController] Instrument not connected");
    qWarning() << "[DPO7000TriggerController] No instrument connected";
}
