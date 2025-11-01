// page3viewmodel.cpp
#include "page3viewmodel.h"
#include <QDebug>
#include "messageservice.h"
#include <QtConcurrent/QtConcurrentRun>
#include "communicationfactory.h"
#include <QMessageBox>
#include <QPointer>
#include "acsourcefactory.h"
#include "dcloadfactory.h"
#include <QTimer>
#include "oscilloscopefactory.h"


Page3ViewModel::Page3ViewModel(Page3Model* p3, QObject *parent)
    : QObject(parent), m_page3(p3) {

    // ===== 初始化防抖計時器 =====
    m_configTimer = new QTimer(this);
    m_configTimer->setSingleShot(true);

    // 當計時器超時時，執行實際的配置更新
    connect(m_configTimer, &QTimer::timeout,
            this, &Page3ViewModel::applyPendingConfig);
}

Page3ViewModel::~Page3ViewModel()
{
    // 停止計時器
    if (m_configTimer) {
        m_configTimer->stop();
    }
    cleanupAllInstruments();
    cleanupTriggerResources();
}

void Page3ViewModel::setMaxOutput(int maxOutput)
{
    if (maxOutput <= 0) return;

    QStringList hdr{ "Output" };
    for (int i = 1; i <= maxOutput; ++i)
        hdr << QString("Index%1").arg(i);
    emit headersChanged(hdr);

}

void Page3ViewModel::setNameList(const QStringList &names)
{
    emit rowLabelsChanged(names);
}

void Page3ViewModel::updateTitles(LoadKind type, const QStringList& titles)
{
    switch (type) {
    case LoadKind::Input:
        if (m_page3) m_page3->setInputTitles(titles);
        m_inputTitles = titles;
        emit TitlesUpdated(LoadKind::Input, titles);
        break;
    case LoadKind::Load:
        if (m_page3) m_page3->setLoadTitles(titles);
        m_loadTitles = titles;
        emit TitlesUpdated(LoadKind::Load, titles);
        break;
    case LoadKind::DyLoad:
        if (m_page3) m_page3->setdyLoadTitles(titles);
        m_dyloadTitles = titles;
        emit TitlesUpdated(LoadKind::DyLoad, titles);
        break;
    case LoadKind::Relay:
        if (m_page3) m_page3->setRelayTitles(titles);
        m_relayTitles = titles;
        emit TitlesUpdated(LoadKind::Relay, titles);
        break;
    }
}

void Page3ViewModel::createAllInstruments() {
    cleanupAllInstruments();
    createOscilloscopes();
}

void Page3ViewModel::cleanupAllInstruments() {
    cleanupOscilloscopes();
}

Oscilloscope* Page3ViewModel::getOscilloscopeByModel(const QString& modelName) {
    return m_oscilloscopes.value(modelName, nullptr);
}

void Page3ViewModel::createOscilloscopes()
{
    // 先清理舊的資源（確保完全釋放）
    cleanupOscilloscopes();

    int createdCount = 0;
    int skippedCount = 0;

    // 遍歷配置，創建所有啟用的示波器
    for (const auto& ic : m_page1Config.instruments) {
        if (ic.type != "Oscilloscope") {
            continue;
        }

        if (!ic.enabled) {
            // qDebug() << "[Page3VM] Skip disabled oscilloscope:" << ic.modelName;
            skippedCount++;
            continue;
        }

        // 驗證配置完整性
        if (ic.modelName.isEmpty() || ic.address.isEmpty()) {
            // qWarning() << "[Page3VM] Invalid config for oscilloscope:"
            //            << ic.name << "(model or address is empty)";
            skippedCount++;
            continue;
        }

        // qDebug() << "[Page3VM] Creating oscilloscope:";
        // qDebug() << "  Name:    " << ic.name;
        // qDebug() << "  Model:   " << ic.modelName;
        // qDebug() << "  Address: " << ic.address;

        // ===== 步驟1: 創建 Communication =====
        ICommunication* comm = CommunicationFactory::create(ic.address);
        if (!comm) {
            // qWarning() << "[Page3VM] Failed to create communication for:" << ic.address;
            skippedCount++;
            continue;
        }
        // qDebug() << "  Communication created";

        // ===== 步驟2: 創建 Oscilloscope =====
        Oscilloscope* oscilloscope = OscilloscopeFactory::createOscilloscope(
            ic.modelName, comm);

        if (!oscilloscope) {
            // qWarning() << "[Page3VM] Failed to create oscilloscope for model:" << ic.modelName;
            delete comm;  // 記得清理 comm
            skippedCount++;
            continue;
        }
        // qDebug() << "  Oscilloscope object created";

        // ===== 步驟3: 連接並驗證 =====
        try {
            oscilloscope->connect();

            if (!oscilloscope->isConnected()) {
                // qWarning() << "[Page3VM] Failed to connect oscilloscope:" << ic.modelName;
                delete oscilloscope;
                delete comm;
                skippedCount++;
                continue;
            }

            // qDebug() << "  Connection established";

        } catch (const std::exception& e) {
            qWarning() << "[Page3VM] Exception during connection:" << e.what();
            delete oscilloscope;
            delete comm;
            skippedCount++;
            continue;
        }

        // ===== 步驟4: 儲存成功創建的儀器 =====
        m_oscilloscopes[ic.modelName] = oscilloscope;
        m_oscilloscopeComms[ic.modelName] = comm;
        createdCount++;

        // 設置第一台為當前活躍儀器
        if (!m_currentOscilloscope) {
            m_currentOscilloscope = oscilloscope;
            m_currentInstrumentModel = ic.modelName;
            // qDebug() << "  Set as current oscilloscope";
        }
    }

    // qDebug() << "[Page3VM] Oscilloscope creation summary:";
    // qDebug() << "  Created: " << createdCount;
    // qDebug() << "  Skipped: " << skippedCount;
    // qDebug() << "[Page3VM] ===== Oscilloscope Creation Complete =====";
}

void Page3ViewModel::cleanupOscilloscopes()
{
    if (m_oscilloscopes.isEmpty() && m_oscilloscopeComms.isEmpty()) {
        // qDebug() << "[Page3VM] No oscilloscopes to clean up";
        return;
    }

    // qDebug() << "[Page3VM] ===== Cleaning Up Oscilloscopes =====";
    // qDebug() << "[Page3VM] Oscilloscopes count:" << m_oscilloscopes.size();

    // ===== 斷開所有連接 =====
    // qDebug() << "[Page3VM] Phase 1: Disconnecting...";
    for (auto it = m_oscilloscopes.begin(); it != m_oscilloscopes.end(); ++it) {
        if (it.value() && it.value()->isConnected()) {
            // qDebug() << "Disconnecting:" << it.key();

            try {
                it.value()->disconnect();
                // qDebug() << "Disconnected";
            } catch (const std::exception& e) {
                qWarning() << "Disconnect failed:" << e.what();
            } catch (...) {
                qWarning() << "Disconnect failed: Unknown error";
            }
        }
    }

    // ===== 等待斷開完成 =====
    // qDebug() << "[Page3VM] Phase 2: Waiting for disconnection to complete...";
    QThread::msleep(100);  // 給儀器時間完成斷開操作

    // ===== 刪除 Oscilloscope 對象 =====
    // qDebug() << "[Page3VM] Phase 3: Deleting oscilloscope objects...";
    for (auto it = m_oscilloscopes.begin(); it != m_oscilloscopes.end(); ++it) {
        if (it.value()) {
            // qDebug() << "  Deleting oscilloscope:" << it.key();

            try {
                delete it.value();
            } catch (const std::exception& e) {
                qWarning() << "Delete failed:" << e.what();
            } catch (...) {
                qWarning() << "Delete failed: Unknown error";
            }
        }
    }
    m_oscilloscopes.clear();

    // ===== 刪除 Communication 對象 =====
    // qDebug() << "[Page3VM] Phase 4: Deleting communication objects...";
    for (auto it = m_oscilloscopeComms.begin(); it != m_oscilloscopeComms.end(); ++it) {
        if (it.value()) {
            // qDebug() << "Deleting communication:" << it.key();

            try {
                delete it.value();
                // qDebug() << "Deleted";
            } catch (const std::exception& e) {
                qWarning() << "Delete failed:" << e.what();
            } catch (...) {
                qWarning() << "Delete failed: Unknown error";
            }
        }
    }
    m_oscilloscopeComms.clear();

    // ===== 清空當前指標 =====
    // qDebug() << "[Page3VM] Phase 5: Clearing pointers...";
    m_currentOscilloscope = nullptr;
    m_currentInstrumentModel.clear();
}

void Page3ViewModel::onPage1ConfigChanged(const Page1Config &cfg)
{
    QMutexLocker locker(&m_stateMutex);

    // 儲存最新配置
    m_pendingConfig = cfg;

        // for (const auto& ic : cfg.instruments) {
        //     qDebug() << "[P3VM] name=" << ic.name << "channelNumbers=" << ic.channelNumbers;
        // }


        //check page1 instrument setting data
        // for (const auto& ic : cfg.instruments)
        //     qDebug() << ic.name << "enabled=" << ic.enabled;

        // qDebug() << "[Page3ViewModel] Received Page1Config:";
        // qDebug() << "  loadOutputs =" << cfg.loadOutputs;
        // qDebug() << "  instruments.count =" << cfg.instruments.size();
        // for (int i = 0; i < cfg.instruments.size(); ++i) {
        //     const InstrumentConfig &ic = cfg.instruments[i];
        //     qDebug() << QString("  Instrument[%1]:").arg(i);
        //     qDebug() << "    name     =" << ic.name;
        //     qDebug() << "    type     =" << ic.type;
        //     qDebug() << "    model    =" << ic.modelName;
        //     qDebug() << "    address  =" << ic.address;
        //     qDebug() << "    enabled  =" << ic.enabled;
        //     qDebug() << "    channels.count =" << ic.channels.size();
        //     for (int j = 0; j < ic.channels.size(); ++j) {
        //         const ChannelSetting &ch = ic.channels[j];
        //         qDebug() << QString("      Channel[%1]: subModel=%2, index=%3")
        //                         .arg(j).arg(ch.subModel).arg(ch.index);
        //     }
        // }
    switch (m_updateState) {
    case ConfigUpdateState::Idle:
        // 從空閒狀態開始新的防抖週期
        m_updateState = ConfigUpdateState::Pending;
        m_configTimer->start(configDelayTime);
        // qDebug() << "[Page3VM] Config update scheduled (Idle → Pending)";
        break;

    case ConfigUpdateState::Pending:
        // 已在等待中，重啟計時器（重置倒數）
        m_configTimer->start(configDelayTime);
        // qDebug() << "[Page3VM] Config update rescheduled (still Pending)";
        break;

    case ConfigUpdateState::Processing:
        // 正在處理中，標記為需要重新處理
        m_updateState = ConfigUpdateState::Pending;
        m_configTimer->start(configDelayTime);
        // qDebug() << "[Page3VM] Config update queued (Processing → Pending)";
        break;
    }
}

void Page3ViewModel::applyPendingConfig()
{
    // ===== 狀態檢查與轉換 =====
    {
        QMutexLocker locker(&m_stateMutex);

        if (m_updateState != ConfigUpdateState::Pending) {
            // qDebug() << "[Page3VM] Skip apply, current state is not Pending";
            return;
        }

        // 切換到處理中狀態
        m_updateState = ConfigUpdateState::Processing;
        // qDebug() << "[Page3VM] State: Pending → Processing";
    }

    // ===== 執行配置更新 =====
    try {
        // 更新 Model
        if (m_page3) {
            m_page3->setPage1ConfigChanged(m_pendingConfig);
        }
        m_page1Config = m_pendingConfig;

        // qDebug() << "[Page3VM] Model updated";

        // 重建所有儀器（包括示波器）
        // 這裡會先清理舊的，再創建新的
        createAllInstruments();

        // qDebug() << "[Page3VM] Instruments recreated";

        // 發出配置已變更信號
        emit page1ConfigChanged(m_pendingConfig);

        // qDebug() << "[Page3VM] Configuration applied successfully";

    } catch (const std::exception& e) {
        // 捕獲標準異常
        QString errorMsg = QString("Config apply failed: %1").arg(e.what());
        qCritical() << "[Page3VM]" << errorMsg;

    } catch (...) {
        // 捕獲未知異常
        QString errorMsg = "Config apply failed: Unknown error";
        qCritical() << "[Page3VM]" << errorMsg;
    }

    // ===== 狀態復原 =====
    {
        QMutexLocker locker(&m_stateMutex);
        m_updateState = ConfigUpdateState::Idle;
        // qDebug() << "[Page3VM] State: Processing → Idle";
    }
}

void Page3ViewModel::onInputDataChanged(const QVector<InputRow>& rows) {
    if (m_page3) m_page3->setPage2InputDataChanged(rows);
    m_page2InputData = rows;

    qDebug() << "[Page3ViewModel] InputRows changed, size:" << rows.size();
    for (int i = 0; i < rows.size(); ++i) {
        const auto& r = rows[i];
        qDebug() << QString("  [%1] vin:%2 frequency:%3 phase:%4").arg(i).arg(r.vin).arg(r.frequency).arg(r.phase);
    }
}

void Page3ViewModel::onLoadMetaChanged(const LoadMetaRow& meta) {
    if (m_page3) m_page3->setPage2LoadMetaDataChanged(meta);
    m_LoadMetaData = meta;

    // qDebug() << "[Page3ViewModel] LoadMeta changed:";
    // auto printVec = [](const char* name, const QVector<QString>& vec) {
    //     qDebug() << QString("    %1: [%2]").arg(name).arg(vec.join(", "));
    // };
    // printVec("modes", meta.modes);
    // printVec("names", meta.names);
    // printVec("vo", meta.vo);
    // printVec("von", meta.von);
    // printVec("riseSlope", meta.riseSlope);
    // printVec("fallSlope", meta.fallSlope);
}

void Page3ViewModel::onLoadRowsChanged(const QVector<LoadDataRow>& rows) {
    if (m_page3) m_page3->setPage2LoadRowsChanged(rows);
    m_LoadRowsData = rows;
    // qDebug() << "[Page3ViewModel] LoadRows changed, size:" << rows.size();
    // for (int i = 0; i < rows.size(); ++i) {
    //     const auto& r = rows[i];
    //     qDebug() << QString("  [%1] label:%2 values:[%3]").arg(i).arg(r.label).arg(r.values.join(", "));
    // }
}

void Page3ViewModel::onDynamicMetaChanged(const DynamicMetaRow& meta) {
    if (m_page3) m_page3->setPage2DynamicMetaChanged(meta);
    m_DynamicMetaData = meta;

    // qDebug() << "[Page3ViewModel] DynamicMeta changed:";
    // auto printVec = [](const char* name, const QVector<QString>& vec) {
    //     qDebug() << QString("    %1: [%2]").arg(name).arg(vec.join(", "));
    // };
    // printVec("von", meta.von);
    // printVec("riseSlope", meta.riseSlope);
    // printVec("fallSlope", meta.fallSlope);
    // printVec("t1t2", meta.t1t2);
}

void Page3ViewModel::onDynamicRowsChanged(const QVector<DynamicDataRow>& rows) {
    if (m_page3) m_page3->setPage2DynamicRowsChanged(rows);
    m_DynamicRowsData = rows;

    // qDebug() << "[Page3ViewModel] DynamicRows changed, size:" << rows.size();
    // for (int i = 0; i < rows.size(); ++i) {
    //     const auto& r = rows[i];
    //     qDebug() << QString("  [%1] label:%2 values:[%3]").arg(i).arg(r.label).arg(r.values.join(", "));
    // }
}


void Page3ViewModel::onInputToggled(bool on)
{
    if (on){
        // qDebug() << "handleInput(InputAction::PowerOn)";
        handleInput(InputAction::PowerOn);
    }
    else{
        // qDebug() << "handleInput(InputAction::PowerOff)";
        handleInput(InputAction::PowerOff);
    }
}

void Page3ViewModel::onInputChanged()
{
     handleInput(InputAction::Change);
}

void Page3ViewModel::onLoadToggled(bool on)
{
    // qDebug() << "[VM] Load toggled:" << on;

    if (on) {
        // qDebug() << "Load ON";
         handleLoad(LoadAction::LoadOn);
    } else {
        // qDebug() << "Load OFF";
         handleLoad(LoadAction::LoadOff);
    }
}

void Page3ViewModel::onLoadChanged()
{
    handleLoad(LoadAction::Change);
}

void Page3ViewModel::applyLoadVonSetting(DCLoad* dcLoad,const int &index,const QVector<QString>& vons)
{
    // 設定 Von
    if (index - 1 < vons.size()) {
        bool ok = false;
        double val = vons[index - 1].toDouble(&ok);
        if (ok) dcLoad->setVon(val);
    }
}

void Page3ViewModel::applyLoadSlopeSetting(DCLoad* dcLoad,
                                           int index,
                                           const QVector<QString>& riseSlopeCCH,
                                           const QVector<QString>& fallSlopeCCH,
                                           const QVector<QString>& riseSlopeCCL,
                                           const QVector<QString>& fallSlopeCCL)
{
    auto applySlope = [&](const QString& mode, const QVector<QString>& vec,
                          auto setFunc) {
        if (index - 1 < vec.size()) {
            bool ok;
            double val = vec[index - 1].toDouble(&ok);
            if (ok) {
                dcLoad->setLoadMode(mode);
                (dcLoad->*setFunc)(val);
            }
        }
    };

    applySlope("CCH", riseSlopeCCH, &DCLoad::setStaticRiseSlope);
    applySlope("CCH", fallSlopeCCH, &DCLoad::setStaticFallSlope);
    applySlope("CCL", riseSlopeCCL, &DCLoad::setStaticRiseSlope);
    applySlope("CCL", fallSlopeCCL, &DCLoad::setStaticFallSlope);
}

void Page3ViewModel::applyLoadValueSettings(DCLoad* dcLoad,
                            int index,
                            double value,
                            const QString& mode,
                            const QVector<QString>& outputVoltages)
{
    int nSegments = dcLoad->getNumSegments();

    if (mode == "CC") {
        StaticCurrentParam param;
        param.levels = QVector<double>(nSegments, value);
        param.enabledMask = QVector<bool>(nSegments, true);

        // 從 outputVoltages 取得對應通道的電壓
        if (index - 1 < outputVoltages.size()) {
            bool ok;
            double voltage = outputVoltages[index - 1].toDouble(&ok);
            param.expectedVoltage = ok ? voltage : 0.0;

            if (ok) {
                qDebug() << "[Page3ViewModel] Channel" << index
                         << "- Current:" << value << "A, Voltage:" << voltage << "V"
                         << "Power:" << (value * voltage) << "W";
            }
        } else {
            param.expectedVoltage = 0.0;
            qWarning() << "[Page3ViewModel] No output voltage for channel" << index;
        }

        dcLoad->setStaticCurrent(param);

    } else if (mode == "CV") {
        // TODO: implement CV
    } else if (mode == "CR") {
        // TODO: implement CR
    } else {
        // 默認CC
        StaticCurrentParam param;
        param.levels = QVector<double>(nSegments, value);
        param.enabledMask = QVector<bool>(nSegments, true);

        if (index - 1 < outputVoltages.size()) {
            bool ok;
            double voltage = outputVoltages[index - 1].toDouble(&ok);
            param.expectedVoltage = ok ? voltage : 0.0;
        } else {
            param.expectedVoltage = 0.0;
        }

        dcLoad->setStaticCurrent(param);
    }

}


void Page3ViewModel::applyLoadSettings(DCLoad* dcLoad,
                                       int index,
                                       double value,
                                       const QString& mode,
                                       const QVector<QString>& vons,
                                       const QVector<QString>& riseSlopeCCH,
                                       const QVector<QString>& fallSlopeCCH,
                                       const QVector<QString>& riseSlopeCCL,
                                       const QVector<QString>& fallSlopeCCL,
                                       const QVector<QString>& outputVoltages)
{
    // int nSegments = dcLoad->getNumSegments();
    dcLoad->setChannel(dcLoad->realChannel());

    // 設定 Von
    applyLoadVonSetting(dcLoad,index,vons);

    // 設定 Slope
    applyLoadSlopeSetting(dcLoad,index,riseSlopeCCH,fallSlopeCCH,riseSlopeCCL,fallSlopeCCL);

    // 設定 Curr
    applyLoadValueSettings(dcLoad,index,value,mode,outputVoltages);


}

void Page3ViewModel::applyDyLoadSettings(DCLoad* dcLoad,
                                         int index,
                                         const QString& value,
                                         const QString& dyTime,
                                         const QVector<QString>& vons,
                                         const QVector<QString>& riseSlopeCCDH,
                                         const QVector<QString>& fallSlopeCCDH,
                                         const QVector<QString>& riseSlopeCCDL,
                                         const QVector<QString>& fallSlopeCCDL,
                                         const QVector<QString>& outputVoltages)
{
    // int nSegments = dcLoad->getNumSegments();
    dcLoad->setChannel(dcLoad->realChannel());

   // 設定 Von
   applyLoadVonSetting(dcLoad,index,vons);

    // 設定 CCDH Rise Slope
   applyDyLoadSlopeSetting(dcLoad,index,riseSlopeCCDH,fallSlopeCCDH,riseSlopeCCDL,fallSlopeCCDL);

  // 設定 Curr
   applyDyLoadValueSettings(dcLoad,index,value,dyTime,outputVoltages);


}

void Page3ViewModel::applyDyLoadSlopeSetting(DCLoad* dcLoad,
                                             int index,
                                             const QVector<QString>& riseSlopeCCDH,
                                             const QVector<QString>& fallSlopeCCDH,
                                             const QVector<QString>& riseSlopeCCDL,
                                             const QVector<QString>& fallSlopeCCDL)
{
    auto applySlope = [&](const QString& mode, const QVector<QString>& vec,
                          auto setFunc) {
        if (index - 1 < vec.size()) {
            bool ok;
            double val = vec[index - 1].toDouble(&ok);
            if (ok) {
                dcLoad->setLoadMode(mode);
                (dcLoad->*setFunc)(val);
            }
        }
    };

    applySlope("CCDH", riseSlopeCCDH, &DCLoad::setDynamicRiseSlope);
    applySlope("CCDH", fallSlopeCCDH, &DCLoad::setDynamicFallSlope);
    applySlope("CCDL", riseSlopeCCDL, &DCLoad::setDynamicRiseSlope);
    applySlope("CCDL", fallSlopeCCDL, &DCLoad::setDynamicFallSlope);
}

void Page3ViewModel::applyDyLoadValueSettings(DCLoad* dcLoad,
                              int index,
                              const QString& value,
                              const QString& dyTime,
                              const QVector<QString>& outputVoltages)
{
    int nSegments = dcLoad->getNumSegments();

    // 動態模式設定
    DynamicCurrentParam param;

    // 解析電流值 (例如: "1.01~3.01")
    QStringList currentParts = value.split('~');
    for (const auto& part : currentParts) {
        bool ok = false;
        double value = part.trimmed().toDouble(&ok);
        if (ok && param.levels.size() < nSegments) {
            param.levels.append(value);
            param.enabledMask.append(true);
        }
    }

    if (param.levels.size() == 1 && nSegments >= 2) {
        param.levels.append(param.levels[0]);
        param.enabledMask.append(true);
    }

    // 解析時間值 (例如: "0.1~0.2" 或 "0.1")
    QStringList timeParts = dyTime.split('~');
    for (const auto& part : timeParts) {
        bool ok = false;
        double value = part.trimmed().toDouble(&ok);
        if (ok && param.timings.size() < 2) {
            param.timings.append(value);
        }
    }

    if (param.timings.size() == 1) {
        param.timings.append(param.timings[0]);
    }

    if (param.timings.isEmpty()) {
        param.timings.append(0.01);
        param.timings.append(0.01);
    }
    //  從 outputVoltages 取得對應通道的電壓
    if (index - 1 < outputVoltages.size()) {
        bool ok;
        double voltage = outputVoltages[index - 1].toDouble(&ok);
        param.expectedVoltage = ok ? voltage : 0.0;

        if (ok && !param.levels.isEmpty()) {
            double maxCurrent = *std::max_element(param.levels.begin(), param.levels.end());
            qDebug() << "[Page3ViewModel] Dynamic Load Channel" << index
                     << "- Max Current:" << maxCurrent << "A, Voltage:" << voltage << "V"
                     << "Max Power:" << (maxCurrent * voltage) << "W";
        }
    } else {
        param.expectedVoltage = 0.0;
        qWarning() << "[Page3ViewModel] No output voltage for dynamic load channel" << index;
    }

    if (!param.levels.isEmpty()) {
        dcLoad->setDynamicCurrent(param);
    }

}


void Page3ViewModel::onDyloadToggled(bool on)
{
    // qDebug() << "[VM] Dy Load toggled:" << on;
    if (on) {
        // qDebug() << "Dy Load ON";
        handleDyLoad(DyLoadAction::DyLoadOn);
    } else {
        // qDebug() << "Dy Load OFF";
        handleDyLoad(DyLoadAction::DyloadOff);
    }

}

void Page3ViewModel::onDyLoadChanged()
{
    handleDyLoad(DyLoadAction::Change);
}

void Page3ViewModel::onSelected(LoadKind type, int idx, const QString& txt) {
    // 添加調試輸出
    // qDebug() << "[Page3ViewModel::onSelected]"
    //          << "type=" << static_cast<int>(type)
    //          << "index=" << idx << "text=" << txt;

    switch (type) {
    case LoadKind::Input:
        m_selectedInputIndex = idx;
        m_selectedInputText  = txt;
        // qDebug() << "  Updated Input selection:" << idx << txt;
        break;
    case LoadKind::Load:
        m_selectedLoadIndex = idx;
        m_selectedLoadText  = txt;
        // qDebug() << "  Updated Load selection:" << idx << txt;
        break;
    case LoadKind::DyLoad:
        m_selectedDyLoadIndex = idx;
        m_selectedDyLoadText  = txt;
        // qDebug() << "  Updated DyLoad selection:" << idx << txt;
        break;
    case LoadKind::Relay:
        m_selectedRelayIndex = idx;
        m_selectedRelayText  = txt;
        break;
    }
}

void Page3ViewModel::onTriggerWidgetCreated(const QString& modelName, QObject* triggerController)
{
    // qDebug() << "[Page3ViewModel] onTriggerWidgetCreated called with modelName:" << modelName;

    cleanupTriggerResources();

    if (auto* abstractController = qobject_cast<AbstractTriggerController*>(triggerController)) {
        m_currentTriggerController = abstractController;
        m_currentInstrumentModel = modelName;

        // qDebug() << "[Page3ViewModel] Set current model to:" << m_currentInstrumentModel;
        // qDebug() << "[Page3ViewModel] Controller supports model:" << abstractController->getSupportedModel();

        if (abstractController->getSupportedModel() == modelName) {
            connectTriggerController();
        } else {
            qWarning() << "[Page3ViewModel] Controller model mismatch:"
                       << abstractController->getSupportedModel() << "vs" << modelName;
        }
    }
}

void Page3ViewModel::onTriggerWidgetDestroyed()
{
    // qDebug() << "[Page3ViewModel] Trigger widget destroyed";
    cleanupTriggerResources();
}

void Page3ViewModel::connectTriggerController() {
    if (!m_currentTriggerController) {
        qWarning() << "[Page3ViewModel] No trigger controller available";
        return;
    }

    // 除錯：檢查容器內容
    // qDebug() << "[Page3ViewModel] Available oscilloscopes:";
    for (auto it = m_oscilloscopes.begin(); it != m_oscilloscopes.end(); ++it) {
        // qDebug() << "  Model:" << it.key() << "Oscilloscope:" << it.value();
    }
    // qDebug() << "[Page3ViewModel] Looking for model:" << m_currentInstrumentModel;

    Oscilloscope* oscilloscope = m_oscilloscopes.value(m_currentInstrumentModel, nullptr);
    if (!oscilloscope) {
        // qWarning() << "[Page3ViewModel] No oscilloscope found for model:" << m_currentInstrumentModel;
        // qWarning() << "[Page3ViewModel] Available models:" << m_oscilloscopes.keys();
        return;
    }

    // 如果執行到這裡，說明找到了示波器物件
    // qDebug() << "[Page3ViewModel] Found oscilloscope, setting to controller";
    m_currentTriggerController->setInstrument(oscilloscope);
    m_currentOscilloscope = oscilloscope;

    // qDebug() << "[Page3ViewModel] Connected trigger controller to"
    //          << oscilloscope->model() << "oscilloscope";
}

void Page3ViewModel::cleanupTriggerResources()
{
    m_currentTriggerController = nullptr;
    m_currentInstrumentModel.clear();
}

void Page3ViewModel::writeXml(QXmlStreamWriter& writer) const
{
    m_page3->setSelectedInputState(m_selectedInputIndex, m_selectedInputText);
    m_page3->setSelectedLoadState(m_selectedLoadIndex, m_selectedLoadText);
    m_page3->setSelectedDyLoadState(m_selectedDyLoadIndex, m_selectedDyLoadText);
    m_page3->setSelectedRelayState(m_selectedRelayIndex, m_selectedRelayText);

    // 添加調試輸出
    // qDebug() << "[Page3ViewModel::writeXml] Saving selections:";
    // qDebug() << "  Input:" << m_selectedInputIndex << m_selectedInputText;
    // qDebug() << "  Load:" << m_selectedLoadIndex << m_selectedLoadText;
    // qDebug() << "  DyLoad:" << m_selectedDyLoadIndex << m_selectedDyLoadText;

    m_page3->writeXml(writer);
}

void Page3ViewModel::loadXml(QXmlStreamReader& reader)
{
    // 先讓 Model 載入 XML 資料
    m_page3->loadXml(reader);

    // 從 Model 取得載入的資料並同步到 ViewModel
    restoreFromModel();

    // 發送信號更新 UI
    updateUIAfterLoad();
}

void Page3ViewModel::restoreFromModel()
{
    if (!m_page3) return;

    // 還原 ComboBox 選項
    m_inputTitles = m_page3->getInputTitles();
    m_loadTitles = m_page3->getLoadTitles();
    m_dyloadTitles = m_page3->getDyLoadTitles();
    m_relayTitles = m_page3->getRelayTitles();

    // 還原 Load 相關資料
    m_LoadMetaData = m_page3->getLoadMetaData();
    m_LoadRowsData = m_page3->getLoadRowsData();
    m_DynamicMetaData = m_page3->getDynamicMetaData();
    m_DynamicRowsData = m_page3->getDynamicRowsData();

    // 還原當前選擇狀態
    m_selectedInputIndex = m_page3->getSelectedInputIndex();
    m_selectedInputText = m_page3->getSelectedInputText();
    m_selectedLoadIndex = m_page3->getSelectedLoadIndex();
    m_selectedLoadText = m_page3->getSelectedLoadText();
    m_selectedDyLoadIndex = m_page3->getSelectedDyLoadIndex();
    m_selectedDyLoadText = m_page3->getSelectedDyLoadText();
    m_selectedRelayIndex = m_page3->getSelectedRelayIndex();
    m_selectedRelayText = m_page3->getSelectedRelayText();

    // qDebug() << "[Page3ViewModel::restoreFromModel] Restored selections:";
    // qDebug() << "  Input:" << m_selectedInputIndex << m_selectedInputText;
    // qDebug() << "  Load:" << m_selectedLoadIndex << m_selectedLoadText;
    // qDebug() << "  DyLoad:" << m_selectedDyLoadIndex << m_selectedDyLoadText;
}

void Page3ViewModel::updateUIAfterLoad()
{
    // 更新 ComboBox 內容
    emit TitlesUpdated(LoadKind::Input, m_inputTitles);
    emit TitlesUpdated(LoadKind::Load, m_loadTitles);
    emit TitlesUpdated(LoadKind::DyLoad, m_dyloadTitles);
    emit TitlesUpdated(LoadKind::Relay, m_relayTitles);

    // 使用 QTimer 延遲還原選擇狀態，確保 ComboBox 先更新完成
    QTimer::singleShot(100, this, [this]() {
        restoreUISelections();
    });
}

void Page3ViewModel::restoreUISelections()
{
    // 透過信號通知 View 還原選擇狀態
    if (m_selectedInputIndex >= 0) {
        emit restoreSelections(LoadKind::Input, m_selectedInputIndex, m_selectedInputText);
    }
    if (m_selectedLoadIndex >= 0) {
        emit restoreSelections(LoadKind::Load, m_selectedLoadIndex, m_selectedLoadText);
    }
    if (m_selectedDyLoadIndex >= 0) {
        emit restoreSelections(LoadKind::DyLoad, m_selectedDyLoadIndex, m_selectedDyLoadText);
    }
    if (m_selectedRelayIndex >= 0) {
        emit restoreSelections(LoadKind::Relay, m_selectedRelayIndex, m_selectedRelayText);
    }

    // qDebug() << "[Page3ViewModel::restoreUISelections] Sent restore signals";
}

// handleInput
void Page3ViewModel::handleInput(InputAction action)
{
    QPointer<Page3ViewModel> self(this);

    // 驗證配置
    if (!validateInputConfiguration()) {
        return;
    }

    // 異步處理
    QFuture<void> future = QtConcurrent::run([cfg = m_page1Config,
                                              inputTxt = m_selectedInputText,
                                              self, action]() {
        try {
            // 檢查對象有效性
            if (!self) {
                return;
            }

            // 創建 AC Source
            auto createResult = self->createACSource(cfg, self);
            if (!createResult.success || !createResult.source || !createResult.comm) {
                return;
            }

            // 解析輸入參數
            auto params = self->parseInputText(inputTxt);
            if (!params.valid) {
                self->cleanupACSourceResources(createResult.source, createResult.comm);
                return;
            }

            // 執行操作
            self->executeACSourceAction(createResult.source, action, params);

            // 清理資源
            self->cleanupACSourceResources(createResult.source, createResult.comm);

        } catch (const std::exception& ex) {
            if (self) {
                qWarning() << "[InputPower] Exception:" << ex.what();
            }
        }
    });
}

// 檢查配置和選擇狀態
bool Page3ViewModel::validateInputConfiguration()
{
    if (m_page1Config.instruments.isEmpty()) {
        MessageService::instance().showWarning("Error Message",
                                               "No instrument settings have been loaded.\nPlease load the configuration first!");
        emit forceOff(LoadKind::Input);
        return false;
    }

    if (m_selectedInputText.trimmed().isEmpty()) {
        MessageService::instance().showWarning("Error Message",
                                               "No input conditions.\nPlease check the input conditions.\n (example : voltage / frequency / phase)");
        emit forceOff(LoadKind::Input);
        return false;
    }

    return true;
}

Page3ViewModel::ACSourceCreationResult Page3ViewModel::createACSource(
    const Page1Config& cfg,
    QPointer<Page3ViewModel> self)
{
    ACSourceCreationResult result;

    bool instrumentFound = false;
    for (const auto& ic : cfg.instruments) {
        if (ic.name != "Source" || ic.type != "InputSource") {
            continue;
        }

        instrumentFound = true;

        // 檢查是否啟用
        if (!ic.enabled) {
            QMetaObject::invokeMethod(&MessageService::instance(), "showWarning", Qt::QueuedConnection,
                                      Q_ARG(QString, "Error Message"),
                                      Q_ARG(QString, "Power is not enabled.\nPlease check the Instruments configuration!"));
            QMetaObject::invokeMethod(self, "forceOff", Qt::QueuedConnection, Q_ARG(LoadKind, LoadKind::Input));
            return result;
        }

        // 檢查配置完整性
        if (ic.modelName.isEmpty() || ic.address.isEmpty()) {
            QMetaObject::invokeMethod(&MessageService::instance(), "showWarning", Qt::QueuedConnection,
                                      Q_ARG(QString, "Error Message"),
                                      Q_ARG(QString, "Instrument model or address not set.\nPlease check the Instruments configuration!"));
            QMetaObject::invokeMethod(self, "forceOff", Qt::QueuedConnection, Q_ARG(LoadKind, LoadKind::Input));
            return result;
        }

        // 創建通信對象
        result.comm = CommunicationFactory::create(ic.address);
        if (!result.comm) {
            QMetaObject::invokeMethod(&MessageService::instance(), "showWarning", Qt::QueuedConnection,
                                      Q_ARG(QString, "Error Message"),
                                      Q_ARG(QString, "Communication format error.\nPlease check the Instruments configuration!"));
            QMetaObject::invokeMethod(self, "forceOff", Qt::QueuedConnection, Q_ARG(LoadKind, LoadKind::Input));
            return result;
        }

        // 創建 AC Source
        result.source = ACSourceFactory::createACSource(ic.modelName, result.comm);
        if (!result.source) {
            delete result.comm;
            result.comm = nullptr;
            QMetaObject::invokeMethod(&MessageService::instance(), "showWarning", Qt::QueuedConnection,
                                      Q_ARG(QString, "Error Message"),
                                      Q_ARG(QString, "AC Source creation failed!"));
            QMetaObject::invokeMethod(self, "forceOff", Qt::QueuedConnection, Q_ARG(LoadKind, LoadKind::Input));
            return result;
        }

        // 連接 AC Source
        result.source->connect();
        if (!result.source->isConnected()) {
            QMetaObject::invokeMethod(&MessageService::instance(), "showWarning", Qt::QueuedConnection,
                                      Q_ARG(QString, "Error Message"),
                                      Q_ARG(QString, result.source->model() + " communication open failed!"));
            QMetaObject::invokeMethod(self, "forceOff", Qt::QueuedConnection, Q_ARG(LoadKind, LoadKind::Input));
            delete result.source;
            delete result.comm;
            result.source = nullptr;
            result.comm = nullptr;
            return result;
        }

        result.success = true;
        break;
    }

    if (!instrumentFound) {
        QMetaObject::invokeMethod(&MessageService::instance(), "showWarning", Qt::QueuedConnection,
                                  Q_ARG(QString, "Error Message"),
                                  Q_ARG(QString, "No power instruments found."));
        QMetaObject::invokeMethod(self, "forceOff", Qt::QueuedConnection, Q_ARG(LoadKind, LoadKind::Input));
    }

    return result;
}

Page3ViewModel::InputParameters Page3ViewModel::parseInputText(const QString& inputText)
{
    InputParameters params;

    QStringList list = inputText.split('/');
    if (list.size() < 3) {
        QMetaObject::invokeMethod(&MessageService::instance(), "showWarning", Qt::QueuedConnection,
                                  Q_ARG(QString, "Error Message"),
                                  Q_ARG(QString, "Input format error. 電壓/頻率/相位 (如: 90/60/0)"));
        return params;
    }

    params.voltage = list.value(0).toDouble();
    params.frequency = list.value(1).toDouble();
    params.phase = list.value(2).toDouble();
    params.valid = true;

    return params;
}

// 執行 AC Source
void Page3ViewModel::executeACSourceAction(
    ACSource* source,
    InputAction action,
    const InputParameters& params)
{
    switch (action) {
    case InputAction::PowerOn:
        source->setVoltage(params.voltage);
        source->setFrequency(params.frequency);
        source->setPhaseOn(params.phase);
        source->setPowerOn();
        break;

    case InputAction::Change:
        source->setVoltage(params.voltage);
        source->setFrequency(params.frequency);
        source->setPhaseOn(params.phase);
        break;

    case InputAction::PowerOff:
        source->setVoltage(0);
        source->setPowerOff();
        break;
    }
}

// Input資源清理
void Page3ViewModel::cleanupACSourceResources(ACSource* source, ICommunication* comm)
{
    if (source) delete source;
    if (comm) delete comm;
}


// handleLoad
void Page3ViewModel::handleLoad(LoadAction action)
{
    QPointer<Page3ViewModel> self(this);

    // 1. 驗證配置
    if (!validateLoadConfiguration()) {
        return;
    }

    // 2. 準備數據
    const auto& meta = m_LoadMetaData;
    const auto& modes = meta.modes;
    const auto& vons = meta.von;
    const auto& riseSlopeCCH = meta.riseSlopeCCH;
    const auto& fallSlopeCCH = meta.fallSlopeCCH;
    const auto& riseSlopeCCL = meta.riseSlopeCCL;
    const auto& fallSlopeCCL = meta.fallSlopeCCL;
    const auto& outputVoltages = meta.vo;

    // 3. 非同步處理
    QFuture<void> future = QtConcurrent::run([cfg = m_page1Config, self,
                                              action,
                                              modes, vons,
                                              riseSlopeCCH, fallSlopeCCH,
                                              riseSlopeCCL, fallSlopeCCL,
                                              outputVoltages]() {
        try {
            // 檢查對象有效性
            if (!self) return;

            // 創建 DC Load 通道
            auto createResult = self->createDCLoads(cfg, self, LoadKind::Load);
            if (!createResult.success || createResult.dcLoads.isEmpty()) {
                return;
            }

            // 尋找選定的 Load 數據
            auto dataInfo = self->findSelectedLoadData(self);

            // 執行每個 DC Load 的操作
            for (DCLoad* dcLoad : createResult.dcLoads) {
                int index = dcLoad->channelIndex();

                self->executeDCLoadAction(
                    dcLoad, action, index, dataInfo,
                    modes, vons,
                    riseSlopeCCH, fallSlopeCCH,
                    riseSlopeCCL, fallSlopeCCL,
                    outputVoltages
                    );
            }

            // 清理資源
            self->cleanupDCLoadResources(createResult.dcLoads, createResult.commMap);

        } catch (const std::exception& ex) {
            if (self) {
                qWarning() << "[Load] Exception:" << ex.what();
            }
        }
    });
}

// 檢查 Load 配置和選擇狀態
bool Page3ViewModel::validateLoadConfiguration()
{
    if (m_page1Config.instruments.isEmpty()) {
        MessageService::instance().showWarning("Error Message",
                                               "No instrument settings have been loaded.\nPlease load the configuration first!");
        emit forceOff(LoadKind::Load);
        return false;
    }

    if (m_selectedLoadText.trimmed().isEmpty()) {
        MessageService::instance().showWarning("Error Message",
                                               "No load conditions selected.\nPlease select load conditions first!");
        emit forceOff(LoadKind::Load);
        return false;
    }

    return true;
}

Page3ViewModel::DCLoadCreationResult Page3ViewModel::createDCLoads(
    const Page1Config& cfg,
    QPointer<Page3ViewModel> self,
    LoadKind kind)
{
    DCLoadCreationResult result;

    try {
        // 遍歷所有儀器配置
        for (const auto& ic : cfg.instruments) {
            // 過濾條件：必須是啟用的 Load 類型
            if (!ic.enabled || ic.type != "Load") continue;
            if (ic.modelName.isEmpty() || ic.address.isEmpty()) continue;

            // 取得或創建通信物件
            ICommunication* comm = result.commMap.value(ic.address, nullptr);
            if (!comm) {
                comm = CommunicationFactory::create(ic.address);
                if (!comm) continue;
                result.commMap[ic.address] = comm;
            }

            // 為每個通道創建 DC Load
            for (int i = 0; i < ic.channels.size(); ++i) {
                const auto& ch = ic.channels[i];
                if (ch.subModel.isEmpty() || ch.index <= 0) continue;

                DCLoad* dcLoad = DCLoadFactory::createDCLoad(ch.subModel, comm);
                if (!dcLoad) continue;

                // 設定通道參數
                int uiIndex = ch.index;
                int hwChannel = ic.channelNumbers.value(i, -1);
                dcLoad->setRealChannel(hwChannel);
                dcLoad->setChannelIndex(uiIndex);

                // 連接檢查
                dcLoad->connect();
                if (!dcLoad->isConnected()) {
                    QMetaObject::invokeMethod(&MessageService::instance(), "showWarning",
                                              Qt::QueuedConnection,
                                              Q_ARG(QString, "Error Message"),
                                              Q_ARG(QString, dcLoad->model() + " communication open failed!"));
                    // QMetaObject::invokeMethod(self, "emitForceOff", Qt::QueuedConnection,
                    //                           Q_ARG(LoadKind, LoadKind::Load));
                    // QMetaObject::invokeMethod(self, "emitForceOff", Qt::QueuedConnection,
                    //                           Q_ARG(LoadKind, LoadKind::DyLoad));
                    emit self->forceOff(kind);

                    delete dcLoad;

                    // 清理已創建的資源
                    for (auto load : result.dcLoads) delete load;
                    for (auto c : result.commMap) delete c;
                    result.dcLoads.clear();
                    result.commMap.clear();
                    return result;
                }

                result.dcLoads.append(dcLoad);
            }
        }

        // 檢查是否有有效的 DC Load
        if (result.dcLoads.isEmpty()) {
            QString errorMsg;
            if (kind == LoadKind::Load) {
                errorMsg = "No valid DC Load channel is enabled or configured!";
            } else if (kind == LoadKind::DyLoad) {
                errorMsg = "No valid DC Load channel is enabled or configured for dynamic load!";
            } else {
                errorMsg = "No valid DC Load channel is enabled or configured!";
            }

            QMetaObject::invokeMethod(&MessageService::instance(), "showWarning",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, "Error Message"),
                                      Q_ARG(QString, errorMsg));

            // QMetaObject::invokeMethod(self, "emitForceOff", Qt::QueuedConnection,
            //                           Q_ARG(LoadKind, LoadKind::Load));
            // QMetaObject::invokeMethod(self, "emitForceOff", Qt::QueuedConnection,
            //                           Q_ARG(LoadKind, LoadKind::DyLoad));
                emit self->forceOff(kind);

            for (auto comm : result.commMap) delete comm;
            result.commMap.clear();
            return result;
        }

        result.success = true;

    } catch (const std::exception& ex) {
        // 異常處理：清理所有資源
        for (auto load : result.dcLoads) delete load;
        for (auto comm : result.commMap) delete comm;
        result.dcLoads.clear();
        result.commMap.clear();
        result.success = false;
        qWarning() << "[createDCLoads] Exception:" << ex.what();
    }

    return result;
}

Page3ViewModel::LoadDataInfo Page3ViewModel::findSelectedLoadData(
    QPointer<Page3ViewModel> self)
{
    LoadDataInfo info;

    if (!self) return info;

    auto it = std::find_if(self->m_LoadRowsData.begin(),
                           self->m_LoadRowsData.end(),
                           [&](const LoadDataRow& row) {
                               return row.label == self->m_selectedLoadText;
                           });

    if (it != self->m_LoadRowsData.end()) {
        info.values = it->values;
        info.found = true;
    }

    return info;
}

// 執行單個 DC Load
void Page3ViewModel::executeDCLoadAction(
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
    const QVector<QString>& outputVoltages)
{
    int realindex = dcLoad->realChannel();

    // Load Off：直接關閉
    if (action == LoadAction::LoadOff) {
        dcLoad->setChannel(realindex);
        dcLoad->setLoadOff();
        return;
    }

    // Load On 或 Change：需要設定參數
    if (!dataInfo.found) return;
    if (index <= 0 || index > dataInfo.values.size()) return;

    QString strValue = dataInfo.values[index - 1].trimmed();
    if (strValue.isEmpty()) return;

    bool ok = false;
    double currval = strValue.toDouble(&ok);
    if (!ok) return;

    // 取得模式（預設為 CC）
    QString mode = (index - 1 < modes.size()) ?
                       modes[index - 1].trimmed().toUpper() : "CC";

    // 設定通道
    dcLoad->setChannel(realindex);

    // 應用所有設定
    applyLoadSettings(dcLoad, index, currval, mode,
                      vons, riseSlopeCCH, fallSlopeCCH,
                      riseSlopeCCL, fallSlopeCCL,
                      outputVoltages);

    // Load On：開啟負載
    if (action == LoadAction::LoadOn) {
        dcLoad->setLoadOn();
    }
}

// 清理 DC Load 資源
void Page3ViewModel::cleanupDCLoadResources(
    QVector<DCLoad*>& dcLoads,
    QMap<QString, ICommunication*>& commMap)
{
    for (auto dcLoad : dcLoads) {
        if (dcLoad) delete dcLoad;
    }
    dcLoads.clear();

    for (auto comm : commMap) {
        if (comm) delete comm;
    }
    commMap.clear();
}

// handleDyLoad
    void Page3ViewModel::handleDyLoad(DyLoadAction action)
{
    QPointer<Page3ViewModel> self(this);

    // 1. 驗證配置
    if (!validateDyLoadConfiguration()) {
        return;
    }

    // 2. 準備數據
    const auto& meta = m_DynamicMetaData;
    const auto& vons = meta.von;
    const auto& riseSlopeCCDH = meta.riseSlopeCCDH;
    const auto& fallSlopeCCDH = meta.fallSlopeCCDH;
    const auto& riseSlopeCCDL = meta.riseSlopeCCDL;
    const auto& fallSlopeCCDL = meta.fallSlopeCCDL;
    const auto& outputVoltages = meta.vo;
    const auto& t1t2 = meta.t1t2;

    // 3. 非同步處理
    QFuture<void> future = QtConcurrent::run([cfg = m_page1Config, self,
                                              action,
                                              vons,
                                              riseSlopeCCDH, fallSlopeCCDH,
                                              riseSlopeCCDL, fallSlopeCCDL,
                                              outputVoltages, t1t2]() {
        try {
            // 檢查對象有效性
            if (!self) return;

            // 4. 創建 DC Load 通道（重用 handleLoad 的函數）
            auto createResult = self->createDCLoads(cfg, self, LoadKind::DyLoad);
            if (!createResult.success || createResult.dcLoads.isEmpty()) {
                return;
            }

            // 5. 尋找選定的 DyLoad 數據
            auto dataInfo = self->findSelectedDyLoadData(self, t1t2);

            // 6. 執行每個 DC Load 的動態操作
            for (DCLoad* dcLoad : createResult.dcLoads) {
                int index = dcLoad->channelIndex();

                self->executeDCDyLoadAction(
                    dcLoad, action, index, dataInfo,
                    vons,
                    riseSlopeCCDH, fallSlopeCCDH,
                    riseSlopeCCDL, fallSlopeCCDL,
                    outputVoltages
                    );
            }

            // 7. 清理資源（重用 handleLoad 的函數）
            self->cleanupDCLoadResources(createResult.dcLoads, createResult.commMap);

        } catch (const std::exception& ex) {
            if (self) {
                qWarning() << "[DynamicLoad] Exception:" << ex.what();
            }
        }
    });
}

// 檢查 DyLoad 配置和選擇狀態
bool Page3ViewModel::validateDyLoadConfiguration()
{
    if (m_page1Config.instruments.isEmpty()) {
        MessageService::instance().showWarning("Error Message",
                                               "No instrument settings have been loaded.\nPlease load the configuration first!");
        emit forceOff(LoadKind::DyLoad);
        return false;
    }

    if (m_selectedDyLoadText.trimmed().isEmpty()) {
        MessageService::instance().showWarning("Error Message",
                                               "No dynamic load conditions selected.\nPlease select dynamic load conditions first!");
        emit forceOff(LoadKind::DyLoad);
        return false;
    }

    return true;
}

// 尋找選定的 DyLoad 數據行
struct DyLoadDataInfo {
    QVector<QString> values;
    QString t1t2;      // 時間參數
    bool found = false;
};

Page3ViewModel::DyLoadDataInfo Page3ViewModel::findSelectedDyLoadData(
    QPointer<Page3ViewModel> self,
    const QVector<QString>& t1t2Vector)
{
    DyLoadDataInfo info;

    if (!self) return info;

    auto it = std::find_if(self->m_DynamicRowsData.begin(),
                           self->m_DynamicRowsData.end(),
                           [&](const DynamicDataRow& row) {
                               return row.label == self->m_selectedDyLoadText;
                           });

    if (it != self->m_DynamicRowsData.end()) {
        info.values = it->values;

        // 從 t1t2 獲取時間數據
        int rowIndex = std::distance(self->m_DynamicRowsData.begin(), it);
        if (rowIndex >= 0 && rowIndex < t1t2Vector.size()) {
            info.t1t2 = t1t2Vector[rowIndex].trimmed();
        }

        info.found = true;
    }

    return info;
}

// 執行單個 DC Load 的動態操作
void Page3ViewModel::executeDCDyLoadAction(
    DCLoad* dcLoad,
    DyLoadAction action,
    int index,
    const DyLoadDataInfo& dataInfo,
    const QVector<QString>& vons,
    const QVector<QString>& riseSlopeCCDH,
    const QVector<QString>& fallSlopeCCDH,
    const QVector<QString>& riseSlopeCCDL,
    const QVector<QString>& fallSlopeCCDL,
    const QVector<QString>& outputVoltages)
{
    int realindex = dcLoad->realChannel();

    // DyLoad Off：直接關閉
    if (action == DyLoadAction::DyloadOff) {
        dcLoad->setChannel(realindex);
        dcLoad->setLoadOff();
        return;
    }

    // DyLoad On 或 Change：需要設定參數
    if (!dataInfo.found) return;
    if (index <= 0 || index > dataInfo.values.size()) return;

    QString strValue = dataInfo.values[index - 1].trimmed();
    if (strValue.isEmpty()) return;

    // 設定通道
    dcLoad->setChannel(realindex);

    // 應用所有動態負載設定
    applyDyLoadSettings(dcLoad, index, strValue, dataInfo.t1t2,
                        vons, riseSlopeCCDH, fallSlopeCCDH,
                        riseSlopeCCDL, fallSlopeCCDL,
                        outputVoltages);

    // DyLoad On：開啟負載
    if (action == DyLoadAction::DyLoadOn) {
        dcLoad->setLoadOn();
    }
}

void Page3ViewModel::emitForceOff(LoadKind kind) {
    emit forceOff(kind);
}
