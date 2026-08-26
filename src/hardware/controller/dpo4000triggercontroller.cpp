#include "dpo4000triggercontroller.h"
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

// ─────────────────────────────────────────────────────────────────────────────
// 建構 / 解構
// ─────────────────────────────────────────────────────────────────────────────

DPO4000TriggerController::DPO4000TriggerController(QWidget* triggerWidget, QObject* parent)
    : AbstractTriggerController(triggerWidget, parent)
{
    if (!triggerWidget) return;

    // 透過 objectName 從 Widget 取得所有子控件指標
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
    m_spinTrigLevel  = triggerWidget->findChild<SmartStepSpinBox*>("triggerLevel");
    m_autoTrigScale  = triggerWidget->findChild<SmartStepSpinBox*>("triggerScale");
    m_autoTrigTarget = triggerWidget->findChild<SmartStepSpinBox*>("triggerTaget");
    m_lblTrigStatus  = triggerWidget->findChild<QLabel*>("triggerStatus");
    m_btnTrigSteady  = triggerWidget->findChild<QPushButton*>("btntrig_Steady");

    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout,
            this, &DPO4000TriggerController::updateRunStopStatus);
    m_statusTimer->start(500);

    connectSignals();
}

DPO4000TriggerController::~DPO4000TriggerController()
{
    cleanup();
}

void DPO4000TriggerController::cleanup()
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
// setInstrument
// ─────────────────────────────────────────────────────────────────────────────

void DPO4000TriggerController::setInstrument(Oscilloscope* instrument)
{
    m_reconnectCounter = 0;
    if (auto* dpo4000 = dynamic_cast<DPO4000*>(instrument)) {
        m_instrument = dpo4000;
        updateTriggerStatus();
    } else if (instrument) {
        qWarning() << "[DPO4000TriggerController] Incompatible instrument type:"
                   << instrument->model();
        m_instrument = nullptr;
        updateTriggerStatus();
    } else {
        // nullptr：示波器被移除（reload config 期間），更新狀態顯示
        m_instrument = nullptr;
        updateTriggerStatus();
    }
}

void DPO4000TriggerController::setInstrument(DPO4000* instrument)
{
    m_instrument = instrument;
    updateTriggerStatus();
}

DPO4000* DPO4000TriggerController::getDPO4000Instrument() const
{
    return dynamic_cast<DPO4000*>(m_instrument);
}

// ─────────────────────────────────────────────────────────────────────────────
// connectSignals
// ─────────────────────────────────────────────────────────────────────────────

void DPO4000TriggerController::connectSignals()
{
    if (m_btnTrigSingle)
        connect(m_btnTrigSingle, &QPushButton::clicked,
                this, &DPO4000TriggerController::onSingleTriggered);

    if (m_btnRunstop)
        connect(m_btnRunstop, &QPushButton::clicked,
                this, &DPO4000TriggerController::onRunStopTriggered);

    if (m_btnTrigAuto)
        connect(m_btnTrigAuto, &QPushButton::clicked,
                this, &DPO4000TriggerController::onAutoTriggered);

    if (m_btnTrigNorm)
        connect(m_btnTrigNorm, &QPushButton::clicked,
                this, &DPO4000TriggerController::onNormTriggered);

    if (m_btnTrigSet)
        connect(m_btnTrigSet, &QPushButton::clicked,
                this, &DPO4000TriggerController::onSetTriggered);

    if (m_cmbTrigType)
        connect(m_cmbTrigType, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &DPO4000TriggerController::onTriggerTypeChanged);

    if (m_cmbTrigSource)
        connect(m_cmbTrigSource, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, &DPO4000TriggerController::onTriggerSourceChanged);

    if (m_btnTrigRising)
        connect(m_btnTrigRising, &QPushButton::clicked,
                this, &DPO4000TriggerController::onSlopeRisingTriggered);

    if (m_btnTrigFalling)
        connect(m_btnTrigFalling, &QPushButton::clicked,
                this, &DPO4000TriggerController::onSlopeFallingTriggered);

    if (m_btnTrigBoth)
        connect(m_btnTrigBoth, &QPushButton::clicked,
                this, &DPO4000TriggerController::onSlopeBothTriggered);

    if (m_btnTrigSteady) {
        connect(m_btnTrigSteady, &QPushButton::toggled,
                this, [this](bool on) {
                    m_btnTrigSteady->setText(on ? tr("Semi-Auto Trigger ON")
                                                : tr("Semi-Auto Trigger OFF"));
                });
        connect(m_btnTrigSteady, &QPushButton::toggled,
                this, &DPO4000TriggerController::onTriggerSteadyToggled);
    }

    if (m_autoTrigTarget)
        connect(m_autoTrigTarget, QOverload<double>::of(&SmartStepSpinBox::valueChanged),
                this, &DPO4000TriggerController::onTargetLevelChanged);

    if (m_autoTrigScale)
        connect(m_autoTrigScale, QOverload<double>::of(&SmartStepSpinBox::valueChanged),
                this, &DPO4000TriggerController::onStepScaleChanged);
}

// ─────────────────────────────────────────────────────────────────────────────
// Slot 實作 — 觸發控制
// ─────────────────────────────────────────────────────────────────────────────

void DPO4000TriggerController::onSingleTriggered()
{
    if (!checkInstrumentConnection()) { showConnectionError(); return; }

    qDebug() << "[DPO4000TriggerController] Single trigger activated";
    m_instrument->single();

    if (m_lblTrigStatus) {
        m_lblTrigStatus->setText("SINGLE");
        m_lblTrigStatus->setStyleSheet(
            "QLabel { background: #ffff7f; color: #000000; "
            "border-radius: 7px; padding: 2px 10px; font-weight: bold; }");
    }

    QTimer::singleShot(200, this, &DPO4000TriggerController::updateRunStopStatus);
}

void DPO4000TriggerController::onRunStopTriggered()
{
    qDebug() << "[DPO4000TriggerController] onRunStopTriggered";

    if (!checkInstrumentConnection()) { showConnectionError(); return; }

    bool running = m_instrument->isRunning();
    qDebug() << "[DPO4000TriggerController] Current state:"
             << (running ? "Running" : "Stopped");

    if (running)
        m_instrument->stop();
    else
        m_instrument->run();

    QTimer::singleShot(100, this, &DPO4000TriggerController::updateRunStopStatus);
}

void DPO4000TriggerController::onAutoTriggered()
{
    if (!checkInstrumentConnection()) { showConnectionError(); return; }
    qDebug() << "[DPO4000TriggerController] Auto mode triggered";
    m_instrument->automode();
}

void DPO4000TriggerController::onNormTriggered()
{
    if (!checkInstrumentConnection()) { showConnectionError(); return; }
    qDebug() << "[DPO4000TriggerController] Normal mode triggered";
    m_instrument->normal();
}

void DPO4000TriggerController::onSetTriggered()
{
    if (!checkInstrumentConnection()) { showConnectionError(); return; }
    double level = m_spinTrigLevel->value();
    qDebug() << "[DPO4000TriggerController] Trigger level set to:" << level;
    m_instrument->setTriggerLevel(level);
}

void DPO4000TriggerController::onTriggerTypeChanged()
{
    if (!checkInstrumentConnection()) return;
    QString type = m_cmbTrigType->currentText();
    qDebug() << "[DPO4000TriggerController] Trigger type changed to:" << type;
    m_instrument->setTriggerType(type);
}

void DPO4000TriggerController::onTriggerSourceChanged()
{
    if (!checkInstrumentConnection()) return;
    QString source = m_cmbTrigSource->currentText();
    qDebug() << "[DPO4000TriggerController] Trigger source changed to:" << source;
    m_instrument->setTriggerSource(source);
}

void DPO4000TriggerController::onSlopeRisingTriggered()
{
    if (!checkInstrumentConnection()) { showConnectionError(); return; }
    qDebug() << "[DPO4000TriggerController] Slope set to RISING";
    m_instrument->setTriggerSlope("RISING");
}

void DPO4000TriggerController::onSlopeFallingTriggered()
{
    if (!checkInstrumentConnection()) { showConnectionError(); return; }
    qDebug() << "[DPO4000TriggerController] Slope set to FALLING";
    m_instrument->setTriggerSlope("FALLING");
}

void DPO4000TriggerController::onSlopeBothTriggered()
{
    if (!checkInstrumentConnection()) { showConnectionError(); return; }
    qDebug() << "[DPO4000TriggerController] Slope set to BOTH";
    m_instrument->setTriggerSlope("BOTH");
}

// ─────────────────────────────────────────────────────────────────────────────
// Run/Stop 狀態更新
// ─────────────────────────────────────────────────────────────────────────────

void DPO4000TriggerController::updateRunStopStatus()
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

void DPO4000TriggerController::setRunningUI(bool running)
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

// ─────────────────────────────────────────────────────────────────────────────
// updateTriggerStatus（連線狀態）
// ─────────────────────────────────────────────────────────────────────────────

void DPO4000TriggerController::updateTriggerStatus()
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

// ─────────────────────────────────────────────────────────────────────────────
// 半自動觸發（Semi-Auto Trigger）
// ─────────────────────────────────────────────────────────────────────────────

void DPO4000TriggerController::onTriggerSteadyToggled(bool on)
{
    qDebug() << QString("[DPO4000TriggerController] Semi-auto trigger toggled: %1")
    .arg(on ? "ON" : "OFF");

    if (on) {
        if (!checkInstrumentConnection()) {
            showConnectionError();
            QSignalBlocker blocker(m_btnTrigSteady);
            m_btnTrigSteady->setChecked(false);
            m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));
            lockTriggerControls(false);
            if (m_lblTrigStatus) {
                m_lblTrigStatus->setText("ERROR: NO CONNECTION");
                m_lblTrigStatus->setStyleSheet(
                    "QLabel { background: #ff7f7f; border-radius:7px; padding:2px 10px; }");
            }
            return;
        }

        DPO4000* dpo4000 = dynamic_cast<DPO4000*>(m_instrument);
        if (!dpo4000) {
            qWarning() << "[DPO4000TriggerController] Invalid instrument type for semi-auto trigger";
            MessageService::instance().showWarning("Error",
                                                   "Semi-auto trigger requires DPO4000 instrument");
            QSignalBlocker blocker(m_btnTrigSteady);
            m_btnTrigSteady->setChecked(false);
            m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));
            lockTriggerControls(false);
            if (m_lblTrigStatus) {
                m_lblTrigStatus->setText("ERROR: INVALID INSTRUMENT");
                m_lblTrigStatus->setStyleSheet(
                    "QLabel { background: #ff7f7f; border-radius:7px; padding:2px 10px; }");
            }
            return;
        }

        try {
            setupWorkerThread();
        } catch (const std::exception& e) {
            qCritical() << "[DPO4000TriggerController] Exception in setupWorkerThread:" << e.what();
            cleanupWorkerThread();
            QSignalBlocker blocker(m_btnTrigSteady);
            m_btnTrigSteady->setChecked(false);
            m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));
            lockTriggerControls(false);
            if (m_lblTrigStatus) {
                m_lblTrigStatus->setText("ERROR");
                m_lblTrigStatus->setStyleSheet(
                    "QLabel { background: #ff7f7f; border-radius:7px; padding:2px 10px; }");
            }
            MessageService::instance().showWarning("Semi-Auto Trigger Error",
                                                   QString("Failed to start: %1").arg(e.what()));
        } catch (...) {
            qCritical() << "[DPO4000TriggerController] Unknown exception in setupWorkerThread";
            cleanupWorkerThread();
            QSignalBlocker blocker(m_btnTrigSteady);
            m_btnTrigSteady->setChecked(false);
            m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));
            lockTriggerControls(false);
            if (m_lblTrigStatus) {
                m_lblTrigStatus->setText("ERROR");
                m_lblTrigStatus->setStyleSheet(
                    "QLabel { background: #ff7f7f; border-radius:7px; padding:2px 10px; }");
            }
            MessageService::instance().showWarning("Semi-Auto Trigger Error",
                                                   "Unknown error occurred");
        }
    } else {
        qDebug() << "[DPO4000TriggerController] Stopping semi-auto trigger";

        if (m_worker)
            m_worker->stopTracking();

        cleanupWorkerThread();
        lockTriggerControls(false);

        if (m_lblTrigStatus) {
            m_lblTrigStatus->setText("STOPPED");
            m_lblTrigStatus->setStyleSheet(
                "QLabel { background: #ffff7f; border-radius:7px; padding:2px 10px; }");
        }
    }
}

void DPO4000TriggerController::onTargetLevelChanged()
{
    if (m_worker && m_autoTrigTarget) {
        double targetLevel = m_autoTrigTarget->value();
        QMetaObject::invokeMethod(m_worker, "setTargetLevel",
                                  Qt::QueuedConnection,
                                  Q_ARG(double, targetLevel));
        qDebug() << QString("[DPO4000TriggerController] Target level updated to: %1V")
                        .arg(targetLevel, 0, 'f', 3);
    }
}

void DPO4000TriggerController::onStepScaleChanged()
{
    if (m_worker && m_autoTrigScale) {
        double stepScale = m_autoTrigScale->value();
        QMetaObject::invokeMethod(m_worker, "setStepScale",
                                  Qt::QueuedConnection,
                                  Q_ARG(double, stepScale));
        qDebug() << QString("[DPO4000TriggerController] Step scale updated to: %1V")
                        .arg(stepScale, 0, 'f', 3);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Worker Thread 管理
// ─────────────────────────────────────────────────────────────────────────────

void DPO4000TriggerController::setupWorkerThread()
{
    if (!checkInstrumentConnection()) {
        showConnectionError();
        QSignalBlocker blocker(m_btnTrigSteady);
        m_btnTrigSteady->setChecked(false);
        m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));
        return;
    }

    if (!m_autoTrigTarget || !m_autoTrigScale || !m_spinTrigLevel) {
        qWarning() << "[DPO4000TriggerController] Required spinboxes not found";
        QSignalBlocker blocker(m_btnTrigSteady);
        m_btnTrigSteady->setChecked(false);
        m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));
        lockTriggerControls(false);
        return;
    }

    if (m_worker || m_workerThread) {
        qWarning() << "[DPO4000TriggerController] Worker already exists, cleaning up first";
        cleanupWorkerThread();
    }

    // AutoTriggerWorker 透過 Oscilloscope 基底介面操作，
    // 因此可直接傳入 DPO4000（只要它繼承自 Oscilloscope）
    DPO4000* dpo4000 = dynamic_cast<DPO4000*>(m_instrument);
    if (!dpo4000) {
        showConnectionError();
        QSignalBlocker blocker(m_btnTrigSteady);
        m_btnTrigSteady->setChecked(false);
        m_btnTrigSteady->setText(tr("Semi-Auto Trigger OFF"));
        lockTriggerControls(false);
        return;
    }

    m_workerThread = new QThread(this);
    m_worker       = new AutoTriggerWorker(dpo4000, nullptr);
    m_worker->moveToThread(m_workerThread);

    m_worker->setStartLevel(m_spinTrigLevel->value());
    m_worker->setTargetLevel(m_autoTrigTarget->value());
    m_worker->setStepScale(m_autoTrigScale->value());

    connect(m_workerThread, &QThread::started,
            m_worker, &AutoTriggerWorker::startTracking);

    connect(m_worker, &AutoTriggerWorker::targetReached,
            this, [this](double finalLevel) {
                qDebug() << QString("[DPO4000TriggerController] Target reached at: %1V")
                .arg(finalLevel, 0, 'f', 3);
                if (m_spinTrigLevel) {
                    QSignalBlocker b(m_spinTrigLevel);
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
                    QSignalBlocker b(m_spinTrigLevel);
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
                qDebug() << QString("[DPO4000TriggerController] Tracking completed: %1 - %2")
                .arg(success ? "Success" : "Failed").arg(message);

                QSignalBlocker b(m_btnTrigSteady);
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
                qWarning() << "[DPO4000TriggerController] Semi-auto trigger error:" << error;

                QSignalBlocker b(m_btnTrigSteady);
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

void DPO4000TriggerController::cleanupWorkerThread()
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

// ─────────────────────────────────────────────────────────────────────────────
// 輔助方法
// ─────────────────────────────────────────────────────────────────────────────

void DPO4000TriggerController::lockTriggerControls(bool lock)
{
    for (QWidget* w : getTriggerControlWidgets())
        if (w) w->setEnabled(!lock);
}

QList<QWidget*> DPO4000TriggerController::getTriggerControlWidgets() const
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
    if (m_spinTrigLevel)  widgets << m_spinTrigLevel;
    return widgets;
}

bool DPO4000TriggerController::checkInstrumentConnection() const
{
    return m_instrument && m_instrument->isConnected();
}

void DPO4000TriggerController::showConnectionError() const
{
    MessageService::instance().showWarning("Error Message",
                                           "[DPO4000TriggerController] Instrument not connected");
    qWarning() << "[DPO4000TriggerController] No instrument connected";
}

QList<int> DPO4000TriggerController::getCheckedChannels() const
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

QString DPO4000TriggerController::getCurrentTriggerSource() const
{
    if (m_cmbTrigSource)
        return m_cmbTrigSource->currentText();
    return "CH1";
}
