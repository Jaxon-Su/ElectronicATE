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


Page3ViewModel::Page3ViewModel(Page3Model* p3, QObject *parent)
    : QObject(parent), m_page3(p3) {}

Page3ViewModel::~Page3ViewModel()
{
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

void Page3ViewModel::createOscilloscopes() {
    cleanupOscilloscopes();

    // qDebug() << "[Page3ViewModel] createOscilloscopes called, processing" << m_page1Config.instruments.size() << "instruments";

    // 遍歷配置，創建所有啟用的示波器
    for (const auto& ic : m_page1Config.instruments) {
        // qDebug() << "[Page3ViewModel] Checking instrument:" << ic.name << "type:" << ic.type << "enabled:" << ic.enabled;
        // qDebug() << "[Page3ViewModel] ModelName:" << ic.modelName << "Address:" << ic.address;
        if (ic.type == "Oscilloscope" && ic.enabled) {
            if (ic.modelName.isEmpty() || ic.address.isEmpty()) {
                // qWarning() << "[Page3ViewModel] Skipping invalid oscilloscope config:"
                //            << "name=" << ic.name
                //            << "modelName=" << ic.modelName
                //            << "address=" << ic.address;
                continue;
            }

            // 使用工廠模式創建
            ICommunication* comm = CommunicationFactory::create(ic.address);
            if (!comm) {
                // qWarning() << "[Page3ViewModel] Failed to create communication for:" << ic.address;
                continue;
            }

            // 關鍵改進：使用工廠和抽象基類
            Oscilloscope* oscilloscope = OscilloscopeFactory::createOscilloscope(ic.modelName, comm);
            if (!oscilloscope) {
                // qWarning() << "[Page3ViewModel] Unsupported oscilloscope model:" << ic.modelName;
                delete comm;
                continue;
            }

            // 連接並驗證
            oscilloscope->connect();
            if (!oscilloscope->isConnected()) {
                qWarning() << "[Page3ViewModel] Failed to connect oscilloscope:" << ic.modelName;
                delete oscilloscope;
                delete comm;
                continue;
            }

            // 存儲到容器中
            m_oscilloscopes[ic.modelName] = oscilloscope;

            // 設置當前活躍的示波器（通常是第一台或指定的）
            if (!m_currentOscilloscope) {
                m_currentOscilloscope = oscilloscope;
                m_currentInstrumentModel = ic.modelName;
            }

            // qDebug() << "[Page3ViewModel] Created oscilloscope:" << ic.modelName << "at" << ic.address;
        }
    }
}

void Page3ViewModel::cleanupOscilloscopes() {
    for (auto it = m_oscilloscopes.begin(); it != m_oscilloscopes.end(); ++it) {
        if (it.value()) {
            it.value()->disconnect();
            delete it.value();
        }
    }
    m_oscilloscopes.clear();
    m_currentOscilloscope = nullptr;
    m_currentInstrumentModel.clear();
}

void Page3ViewModel::onPage1ConfigChanged(const Page1Config &cfg) {
    if (m_page3) m_page3->setPage1ConfigChanged(cfg);
    m_page1Config = cfg;

    // 統一創建所有儀器（包括示波器）
    createAllInstruments();

    emit page1ConfigChanged(cfg);


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

void Page3ViewModel::handleInput(InputAction action)
{
    // this = 指向當前 Page3ViewModel 實例的指針
    // 如果 this 對象被刪除，self 會自動變成 nullptr
    QPointer<Page3ViewModel> self(this);

    // --- 防呆 ---
    //page1 source
    if (m_page1Config.instruments.isEmpty()) {
        MessageService::instance().showWarning("Error Message",
                                               "No instrument settings have been loaded.\nPlease load the configuration first!");
        emit forceOff(LoadKind::Input);
        return;
    }
    //page3 Input ComboBox
    qDebug() << "m_selectedInputText:" << m_selectedInputText;
    qDebug() << "m_selectedInputIndex:" << m_selectedInputIndex;
    // m_selectedInputText: "";
    // m_selectedInputIndex: -1
    if (m_selectedInputText.trimmed().isEmpty()) {
        MessageService::instance().showWarning("Error Message",
                                               "No input conditions.\nPlease check the input conditions.\n (example : voltage / frequency / phase)");
        emit forceOff(LoadKind::Input);
        return;
    }

    // --- 非同步處理 ---
    QFuture<void> future = QtConcurrent::run([cfg = m_page1Config,
                                              inputTxt = m_selectedInputText,
                                              self, action = static_cast<int>(action)]() {
        try {
            // 使用指針前檢查是否為nullptr
            if (!self) {
                return;
            }
            //enum class InputAction { PowerOn, PowerOff, Change };
            //InputAction::PowerOn = 0 , InputAction::PowerOff = 1 ,  InputAction::Change = 2
            //捕獲InputAction時轉為int，再轉回InputAction type
            //可能可直接捕獲，待確認by Jaxon
            InputAction realAction = static_cast<InputAction>(action);
            // qDebug() << "realAction:" << static_cast<int>(realAction);

            // --- 嚴謹條件建立 AC Source ---
            ACSource* ac_source = nullptr;
            ICommunication* comm = nullptr;
            bool instrumentFound = false;
            for (const auto& ic : cfg.instruments) {
                if (ic.name == "Source" && ic.type == "InputSource") {
                    instrumentFound = true;

                    // QMetaObject::invokeMethod(
                    //     self,                    // 目標對象 >> 這裡是page3viewmodel
                    //     "showWarning",          // 要調用的方法名（字符串）
                    //     Qt::QueuedConnection,   // 連接類型（異步隊列調用）
                    //     Q_ARG(QString, "Error Message"),        // 第1個參數
                    //     Q_ARG(QString, "Power is not enabled...") // 第2個參數
                    //     );

                    //Qt 框架的一個重要限製：所有的 UI 相關操作（界面顯示、更新、事件處理等）都必須在主線程中進行。

                    if (!ic.enabled) {
                        QMetaObject::invokeMethod(&MessageService::instance(), "showWarning", Qt::QueuedConnection,
                                                  Q_ARG(QString, "Error Message"),
                                                  Q_ARG(QString, "Power is not enabled.\nPlease check the Instruments configuration!"));
                        QMetaObject::invokeMethod(self, "forceOff", Qt::QueuedConnection, Q_ARG(LoadKind, LoadKind::Input));
                        return;
                    }

                    if (ic.modelName.isEmpty() || ic.address.isEmpty()) {
                        QMetaObject::invokeMethod(&MessageService::instance(), "showWarning", Qt::QueuedConnection,
                                                  Q_ARG(QString, "Error Message"),
                                                  Q_ARG(QString, "Instrument model or address not set.\nPlease check the Instruments configuration!"));
                        QMetaObject::invokeMethod(self, "forceOff", Qt::QueuedConnection, Q_ARG(LoadKind, LoadKind::Input));
                        return;
                    }

                    comm = CommunicationFactory::create(ic.address);
                    if (!comm) {
                        QMetaObject::invokeMethod(&MessageService::instance(), "showWarning", Qt::QueuedConnection,
                                                  Q_ARG(QString, "Error Message"),
                                                  Q_ARG(QString, "Communication format error.\nPlease check the Instruments configuration!"));
                        QMetaObject::invokeMethod(self, "forceOff", Qt::QueuedConnection, Q_ARG(LoadKind, LoadKind::Input));
                        return;
                    }

                    ac_source = ACSourceFactory::createACSource(ic.modelName, comm);
                    if (!ac_source) {
                        delete comm;
                        QMetaObject::invokeMethod(&MessageService::instance(), "showWarning", Qt::QueuedConnection,
                                                  Q_ARG(QString, "Error Message"),
                                                  Q_ARG(QString, "AC Source creation failed!"));
                        QMetaObject::invokeMethod(self, "forceOff", Qt::QueuedConnection, Q_ARG(LoadKind, LoadKind::Input));
                        return;
                    }
                    ac_source->connect();
                    if (!ac_source->isConnected()) {
                        QMetaObject::invokeMethod(&MessageService::instance(), "showWarning", Qt::QueuedConnection,
                                                  Q_ARG(QString, "Error Message"),
                                                  Q_ARG(QString, ac_source->model() + " communication open failed!"));
                        QMetaObject::invokeMethod(self, "forceOff", Qt::QueuedConnection, Q_ARG(LoadKind, LoadKind::Input));
                        delete ac_source;
                        delete comm;
                        return;
                    }
                    break;
                }
            }
            if (!instrumentFound) {
                QMetaObject::invokeMethod(&MessageService::instance(), "showWarning", Qt::QueuedConnection,
                                          Q_ARG(QString, "Error Message"),
                                          Q_ARG(QString, "No power instruments found."));
                QMetaObject::invokeMethod(self, "forceOff", Qt::QueuedConnection, Q_ARG(LoadKind, LoadKind::Input));
                return;
            }
            if (!ac_source || !comm){
                // qDebug() << "AC source or communication null!";
                return;
            }

            // 解析輸入
            QStringList list = inputTxt.split('/');
            if (list.size() < 3) {
                QMetaObject::invokeMethod(&MessageService::instance(), "showWarning", Qt::QueuedConnection,
                                          Q_ARG(QString, "Error Message"),
                                          Q_ARG(QString, "Input format error. 電壓/頻率/相位 (如: 90/60/0)"));
                delete ac_source;
                delete comm;
                return;
            }

            double volt    = list.value(0).toDouble();
            double freq    = list.value(1).toDouble();
            double phaseOn = list.value(2).toDouble();

            // --- 依 action 執行 ---
            if (realAction == InputAction::PowerOn) {
                ac_source->setVoltage(volt);
                ac_source->setFrequency(freq);
                ac_source->setPhaseOn(phaseOn);
                ac_source->setPowerOn();
                // qDebug() << "Set input power ON: volt=" << volt << ", freq=" << freq << ", phase=" << phaseOn;
            } else if (realAction == InputAction::Change) {
                ac_source->setVoltage(volt);
                ac_source->setFrequency(freq);
                ac_source->setPhaseOn(phaseOn);
                // qDebug() << "Set input param changed: volt=" << volt << ", freq=" << freq << ", phase=" << phaseOn;
            } else if (realAction == InputAction::PowerOff) {
                ac_source->setVoltage(0);
                ac_source->setPowerOff();
                // qDebug() << "Set input power OFF: voltage=0, PowerOff";
            }

            delete ac_source;
            delete comm;
        } catch (const std::exception& ex) {
            if (self)
                qWarning() << "[InputPower] Exception:" << ex.what();
        }
    });

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

void Page3ViewModel::handleLoad(LoadAction action)
{
    QPointer<Page3ViewModel> self(this);

    if (m_page1Config.instruments.isEmpty()) {
        MessageService::instance().showWarning("Error Message",
                                               "No instrument settings have been loaded.\nPlease load the configuration first!");
        emit forceOff(LoadKind::Load);
        return;
    }

    if (m_selectedLoadText.trimmed().isEmpty()) {
        MessageService::instance().showWarning("Error Message",
                                               "No load conditions selected.\nPlease select load conditions first!");
        emit forceOff(LoadKind::Load);
        return;
    }

    const auto& meta = m_LoadMetaData;
    const auto& modes = meta.modes;
    const auto& vons = meta.von;
    const auto& riseSlope = meta.riseSlope;
    const auto& fallSlope = meta.fallSlope;
    const auto& outputVoltages = meta.vo; // 取得輸出電壓

    QFuture<void> future = QtConcurrent::run([cfg = m_page1Config, self,
                                              action = static_cast<int>(action),
                                              modes, vons, riseSlope, fallSlope,
                                              outputVoltages, this]() {
        QVector<DCLoad*> dcLoads;
        QMap<QString, ICommunication*> commMap;

        try {
            // 1. 建立所有合法 DCLoad 通道
            for (const auto& ic : cfg.instruments) {
                if (!ic.enabled || ic.type != "Load") continue;
                if (ic.modelName.isEmpty() || ic.address.isEmpty()) continue;

                ICommunication* comm = commMap.value(ic.address, nullptr);
                if (!comm) {
                    comm = CommunicationFactory::create(ic.address);
                    if (!comm) continue;
                    commMap[ic.address] = comm;
                }

                for (int i = 0; i < ic.channels.size(); ++i) {
                    const auto& ch = ic.channels[i];
                    if (ch.subModel.isEmpty() || ch.index <= 0) continue;

                    DCLoad* dcLoad = DCLoadFactory::createDCLoad(ch.subModel, comm);
                    if (!dcLoad) continue;

                    int uiIndex = ch.index;
                    int hwChannel = ic.channelNumbers.value(i, -1);

                    dcLoad->setRealChannel(hwChannel);
                    dcLoad->setChannelIndex(uiIndex);
                    dcLoad->connect();

                    if (!dcLoad->isConnected()) {
                        QMetaObject::invokeMethod(&MessageService::instance(), "showWarning",
                                                  Qt::QueuedConnection,
                                                  Q_ARG(QString, "Error Message"),
                                                  Q_ARG(QString, dcLoad->model() + " communication open failed!"));
                        QMetaObject::invokeMethod(self, "forceOff", Qt::QueuedConnection,
                                                  Q_ARG(LoadKind, LoadKind::Load));
                        delete dcLoad;
                        return;
                    }

                    dcLoads.append(dcLoad);
                }
            }

            if (dcLoads.isEmpty()) {
                QMetaObject::invokeMethod(&MessageService::instance(), "showWarning",
                                          Qt::QueuedConnection,
                                          Q_ARG(QString, "Error Message"),
                                          Q_ARG(QString, "No valid DC Load channel is enabled or configured!"));
                QMetaObject::invokeMethod(self, "forceOff", Qt::QueuedConnection,
                                          Q_ARG(LoadKind, LoadKind::Load));
                for (auto comm : commMap) delete comm;
                return;
            }

            // 2. 執行 LoadAction
            for (DCLoad* dcLoad : dcLoads) {
                LoadAction act = static_cast<LoadAction>(action);
                int realindex = dcLoad->realChannel();
                int index = dcLoad->channelIndex();

                if (act == LoadAction::LoadOff) {
                    dcLoad->setChannel(realindex);
                    dcLoad->setLoadOff();
                    continue;
                }

                if (!self) continue;

                auto it = std::find_if(self->m_LoadRowsData.begin(),
                                       self->m_LoadRowsData.end(),
                                       [&](const LoadDataRow& row) {
                                           return row.label == self->m_selectedLoadText;
                                       });

                if (it == self->m_LoadRowsData.end()) continue;

                const auto& values = it->values;
                dcLoad->setChannel(realindex);

                if (index <= 0 || index > values.size()) continue;

                QString strValue = values[index - 1].trimmed();
                if (strValue.isEmpty()) continue;

                bool ok = false;
                double currval = strValue.toDouble(&ok);
                if (!ok) continue;

                QString mode = (index - 1 < modes.size()) ?
                                   modes[index - 1].trimmed().toUpper() : "CC";

                // 傳入輸出電壓
                applyLoadSettings(dcLoad, index, currval, mode,
                                  vons, riseSlope, fallSlope, outputVoltages);

                if (act == LoadAction::LoadOn)
                    dcLoad->setLoadOn();
            }

            // 3. 清除資源
            for (auto dcLoad : dcLoads) delete dcLoad;
            for (auto comm : commMap) delete comm;

        } catch (const std::exception& ex) {
            for (auto dcLoad : dcLoads) delete dcLoad;
            for (auto comm : commMap) delete comm;
            if (self)
                qWarning() << "[Load] Exception:" << ex.what();
        }
    });
}

void Page3ViewModel::applyLoadSettings(DCLoad* dcLoad,
                                       int index,
                                       double currval,
                                       const QString& mode,
                                       const QVector<QString>& vons,
                                       const QVector<QString>& riseSlope,
                                       const QVector<QString>& fallSlope,
                                       const QVector<QString>& outputVoltages)
{
    int nSegments = dcLoad->getNumSegments();
    dcLoad->setChannel(dcLoad->realChannel());

    // 設定 Von
    if (index - 1 < vons.size()) {
        bool ok = false;
        double val = vons[index - 1].toDouble(&ok);
        if (ok) dcLoad->setVon(val);
    }

    // 設定 Rise Slope
    if (index - 1 < riseSlope.size()) {
        bool ok = false;
        double val = riseSlope[index - 1].toDouble(&ok);
        if (ok) dcLoad->setStaticRiseSlope(val);
    }

    // 設定 Fall Slope
    if (index - 1 < fallSlope.size()) {
        bool ok = false;
        double val = fallSlope[index - 1].toDouble(&ok);
        if (ok) dcLoad->setStaticFallSlope(val);
    }

    if (mode == "CC") {
        StaticCurrentParam param;
        param.levels = QVector<double>(nSegments, currval);
        param.enabledMask = QVector<bool>(nSegments, true);

        // 從 outputVoltages 取得對應通道的電壓
        if (index - 1 < outputVoltages.size()) {
            bool ok;
            double voltage = outputVoltages[index - 1].toDouble(&ok);
            param.expectedVoltage = ok ? voltage : 0.0;

            if (ok) {
                qDebug() << "[Page3ViewModel] Channel" << index
                         << "- Current:" << currval << "A, Voltage:" << voltage << "V"
                         << "Power:" << (currval * voltage) << "W";
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
        param.levels = QVector<double>(nSegments, currval);
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

void Page3ViewModel::handleDyLoad(DyLoadAction action)
{
    QPointer<Page3ViewModel> self(this);

    if (m_page1Config.instruments.isEmpty()) {
        MessageService::instance().showWarning("Error Message",
                                               "No instrument settings have been loaded.\nPlease load the configuration first!");
        emit forceOff(LoadKind::DyLoad);
        return;
    }

    if (m_selectedDyLoadText.trimmed().isEmpty()) {
        MessageService::instance().showWarning("Error Message",
                                               "No dynamic load conditions selected.\nPlease select dynamic load conditions first!");
        emit forceOff(LoadKind::DyLoad);
        return;
    }

    const auto& meta = m_DynamicMetaData;
    const auto& vons = meta.von;
    const auto& riseSlope = meta.riseSlope;
    const auto& fallSlope = meta.fallSlope;
    const auto& outputVoltages = meta.vo;
    const auto& t1t2 = meta.t1t2;

    QFuture<void> future = QtConcurrent::run([cfg = m_page1Config, self,
                                              action = static_cast<int>(action),
                                              vons, riseSlope, fallSlope,
                                              outputVoltages, t1t2, this]() {
        QVector<DCLoad*> dcLoads;
        QMap<QString, ICommunication*> commMap;

        try {
            // 1. 建立所有合法 DCLoad 通道 (同 handleLoad)
            for (const auto& ic : cfg.instruments) {
                if (!ic.enabled || ic.type != "Load") continue;
                if (ic.modelName.isEmpty() || ic.address.isEmpty()) continue;

                ICommunication* comm = commMap.value(ic.address, nullptr);
                if (!comm) {
                    comm = CommunicationFactory::create(ic.address);
                    if (!comm) continue;
                    commMap[ic.address] = comm;
                }

                for (int i = 0; i < ic.channels.size(); ++i) {
                    const auto& ch = ic.channels[i];
                    if (ch.subModel.isEmpty() || ch.index <= 0) continue;

                    DCLoad* dcLoad = DCLoadFactory::createDCLoad(ch.subModel, comm);
                    if (!dcLoad) continue;

                    int uiIndex = ch.index;
                    int hwChannel = ic.channelNumbers.value(i, -1);

                    dcLoad->setRealChannel(hwChannel);
                    dcLoad->setChannelIndex(uiIndex);
                    dcLoad->connect();

                    if (!dcLoad->isConnected()) {
                        QMetaObject::invokeMethod(&MessageService::instance(), "showWarning",
                                                  Qt::QueuedConnection,
                                                  Q_ARG(QString, "Error Message"),
                                                  Q_ARG(QString, dcLoad->model() + " communication open failed!"));
                        QMetaObject::invokeMethod(self, "forceOff", Qt::QueuedConnection,
                                                  Q_ARG(LoadKind, LoadKind::DyLoad));
                        delete dcLoad;
                        return;
                    }

                    dcLoads.append(dcLoad);
                }
            }

            if (dcLoads.isEmpty()) {
                QMetaObject::invokeMethod(&MessageService::instance(), "showWarning",
                                          Qt::QueuedConnection,
                                          Q_ARG(QString, "Error Message"),
                                          Q_ARG(QString, "No valid DC Load channel is enabled or configured for dynamic load!"));
                QMetaObject::invokeMethod(self, "forceOff", Qt::QueuedConnection,
                                          Q_ARG(LoadKind, LoadKind::DyLoad));
                for (auto comm : commMap) delete comm;
                return;
            }

            // 2. 執行 DyLoadAction
            for (DCLoad* dcLoad : dcLoads) {
                DyLoadAction act = static_cast<DyLoadAction>(action);
                int realindex = dcLoad->realChannel();
                int index = dcLoad->channelIndex();

                if (act == DyLoadAction::DyloadOff) {
                    dcLoad->setChannel(realindex);
                    dcLoad->setLoadOff();
                    continue;
                }

                if (!self) continue;

                auto it = std::find_if(self->m_DynamicRowsData.begin(),
                                       self->m_DynamicRowsData.end(),
                                       [&](const DynamicDataRow& row) {
                                           return row.label == self->m_selectedDyLoadText;
                                       });

                if (it == self->m_DynamicRowsData.end()) continue;

                const auto& values = it->values;
                dcLoad->setChannel(realindex);

                if (index <= 0 || index > values.size()) continue;

                QString strValue = values[index - 1].trimmed();
                if (strValue.isEmpty()) continue;

                // 從 t1t2 獲取時間數據
                // 找到當前選中的 DynamicDataRow 在所有 rows 中的索引
                int rowIndex = std::distance(self->m_DynamicRowsData.begin(), it);
                QString dyTime = "";
                if (rowIndex >= 0 && rowIndex < t1t2.size()) {
                    dyTime = t1t2[rowIndex].trimmed();
                }

                QString mode = "CCDL";

                // 傳入輸出電壓和時間
                applyDyLoadSettings(dcLoad, index, strValue, dyTime, mode,
                                    vons, riseSlope, fallSlope, outputVoltages);

                if (act == DyLoadAction::DyLoadOn) {
                    dcLoad->setLoadOn();
                }
            }

            // 3. 清除資源
            for (auto dcLoad : dcLoads) delete dcLoad;
            for (auto comm : commMap) delete comm;

        } catch (const std::exception& ex) {
            for (auto dcLoad : dcLoads) delete dcLoad;
            for (auto comm : commMap) delete comm;
            if (self)
                qWarning() << "[DynamicLoad] Exception:" << ex.what();
        }
    });
}

void Page3ViewModel::applyDyLoadSettings(DCLoad* dcLoad,
                                         int index,
                                         const QString& currval,
                                         const QString& dyTime,
                                         const QString& mode,
                                         const QVector<QString>& vons,
                                         const QVector<QString>& riseSlope,
                                         const QVector<QString>& fallSlope,
                                         const QVector<QString>& outputVoltages)
{
    int nSegments = dcLoad->getNumSegments();
    dcLoad->setChannel(dcLoad->realChannel());

    // 設定 Von
    if (index - 1 < vons.size()) {
        bool ok = false;
        double val = vons[index - 1].toDouble(&ok);
        if (ok) dcLoad->setVon(val);
    }

    // 設定 Rise Slope
    if (index - 1 < riseSlope.size()) {
        bool ok = false;
        double val = riseSlope[index - 1].toDouble(&ok);
        if (ok) dcLoad->setDynamicRiseSlope(val);
    }

    // 設定 Fall Slope
    if (index - 1 < fallSlope.size()) {
        bool ok = false;
        double val = fallSlope[index - 1].toDouble(&ok);
        if (ok) dcLoad->setDynamicFallSlope(val);
    }

    // 動態模式設定
    DynamicCurrentParam param;

    // 解析電流值 (例如: "1.01~3.01")
    QStringList currentParts = currval.split('~');
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
        qWarning() << "[Page3ViewModel] No oscilloscope found for model:" << m_currentInstrumentModel;
        qWarning() << "[Page3ViewModel] Available models:" << m_oscilloscopes.keys();
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
