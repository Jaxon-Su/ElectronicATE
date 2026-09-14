#include "instrumentcreator.h"
#include "acsource.h"
#include "dcload.h"
#include "relay.h"
#include "icommunication.h"
#include "communicationfactory.h"
#include "acsourcefactory.h"
#include "dcloadfactory.h"
#include "relayfactory.h"
#include "messageservice.h"
#include <QMetaObject>
#include <QRegularExpression>
#include <QDebug>

// ========================================
// Create AC Source
// ========================================

InstrumentCreator::ACSourceResult
InstrumentCreator::createACSource(
    const Page1Config& config,
    QObject* viewModel)
{
    ACSourceResult result;

    bool instrumentFound = false;

    for (const auto& ic : config.instruments) {
        const QString resource = ic.getResourceString();
        // 只處理 Source 類型的儀器
        if (ic.name != "ACSource" || ic.type != "InputSource") {
            continue;
        }

        instrumentFound = true;

        // ===== 檢查 1: 是否啟用 =====
        if (!ic.enabled) {
            QMetaObject::invokeMethod(&MessageService::instance(), "showWarning",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, "Error Message"),
                                      Q_ARG(QString, "Power is not enabled.\nPlease check the Instruments configuration!"));
            if (viewModel) QMetaObject::invokeMethod(viewModel, "forceOff",
                                      Qt::QueuedConnection,
                                      Q_ARG(TableKind, TableKind::Input));
            return result;
        }

        // ===== 檢查 2: 配置完整性 =====
        if (ic.modelName.isEmpty() || resource.isEmpty()) {
            QMetaObject::invokeMethod(&MessageService::instance(), "showWarning",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, "Error Message"),
                                      Q_ARG(QString, "Instrument model or address not set.\nPlease check the Instruments configuration!"));
            if (viewModel) QMetaObject::invokeMethod(viewModel, "forceOff",
                                      Qt::QueuedConnection,
                                      Q_ARG(TableKind, TableKind::Input));
            return result;
        }

        // ===== 步驟 1: 創建通信對象 =====
        result.comm = CommunicationFactory::create(resource);
        if (!result.comm) {
            QMetaObject::invokeMethod(&MessageService::instance(), "showWarning",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, "Error Message"),
                                      Q_ARG(QString, "Communication format error.\nPlease check the Instruments configuration!"));
            if (viewModel) QMetaObject::invokeMethod(viewModel, "forceOff",
                                      Qt::QueuedConnection,
                                      Q_ARG(TableKind, TableKind::Input));
            return result;
        }

        // ===== 步驟 2: 創建 AC Source =====
        result.source = ACSourceFactory::createACSource(ic.modelName, result.comm);
        if (!result.source) {
            delete result.comm;
            result.comm = nullptr;
            QMetaObject::invokeMethod(&MessageService::instance(), "showWarning",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, "Error Message"),
                                      Q_ARG(QString, "AC Source creation failed!"));
            if (viewModel) QMetaObject::invokeMethod(viewModel, "forceOff",
                                      Qt::QueuedConnection,
                                      Q_ARG(TableKind, TableKind::Input));
            return result;
        }

        // ===== 步驟 3: 連接 AC Source =====
        result.source->connect();
        if (!result.source->isConnected()) {
            QString errorMsg = result.source->model() + " communication open failed!";
            QMetaObject::invokeMethod(&MessageService::instance(), "showWarning",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, "Error Message"),
                                      Q_ARG(QString, errorMsg));
            if (viewModel) QMetaObject::invokeMethod(viewModel, "forceOff",
                                      Qt::QueuedConnection,
                                      Q_ARG(TableKind, TableKind::Input));
            delete result.source;
            delete result.comm;
            result.source = nullptr;
            result.comm = nullptr;
            return result;
        }

        // ===== 成功 =====
        result.success = true;
        break;
    }

    // ===== 檢查是否找到儀器 =====
    if (!instrumentFound) {
        QMetaObject::invokeMethod(&MessageService::instance(), "showWarning",
                                  Qt::QueuedConnection,
                                  Q_ARG(QString, "Error Message"),
                                  Q_ARG(QString, "No power instruments found."));
        QMetaObject::invokeMethod(viewModel, "forceOff",
                                  Qt::QueuedConnection,
                                  Q_ARG(TableKind, TableKind::Input));
    }

    return result;
}

// ========================================
// Create DC Loads
// ========================================

InstrumentCreator::DCLoadResult
InstrumentCreator::createDCLoads(
    const Page1Config& config,
    QObject* viewModel,
    TableKind kind)
{
    DCLoadResult result;

    try {
        // ===== 遍歷所有儀器配置 =====
        for (const auto& ic : config.instruments) {
        const QString resource = ic.getResourceString();
            // 過濾條件：必須是啟用的 Load 類型
            if (!ic.enabled || ic.type != "Load") continue;
            if (ic.modelName.isEmpty() || resource.isEmpty()) continue;

            // ===== 獲取或創建通信對象 =====
            ICommunication* comm = result.commMap.value(resource, nullptr);
            if (!comm) {
                comm = CommunicationFactory::create(resource);
                if (!comm) continue;
                result.commMap[resource] = comm;
            }

            // ===== 為每個通道創建 DC Load =====
            for (int i = 0; i < ic.channels.size(); ++i) {
                const auto& ch = ic.channels[i];
                if (ch.subModel.isEmpty() || ch.index <= 0) continue;

                // 創建 DC Load
                // qDebug() << "ch.subModel" << ch.subModel;
                DCLoad* dcLoad = DCLoadFactory::createDCLoad(ch.subModel, comm);
                if (!dcLoad) continue;

                // 設置通道參數
                int uiIndex = ch.index;
                int hwChannel = ic.channelNumbers.value(i, -1);
                dcLoad->setRealChannel(hwChannel);
                dcLoad->setChannelIndex(uiIndex);
                dcLoad->setAddress(resource);
                dcLoad->setConfiguredSyncType(ch.syncType);

                qDebug() << "[InstrumentCreator] DCLoad created:"
                         << ic.name
                         << "slot" << i
                         << "subModel=" << ch.subModel
                         << "| CHAN(realChannel)=" << hwChannel
                         << "| outputIndex(channelIndex)=" << uiIndex
                         << "| configuredSyncType=" << ch.syncType;

                // 連接檢查
                dcLoad->connect();
                if (!dcLoad->isConnected()) {
                    QString errorMsg = dcLoad->model() + " communication open failed!";
                    QMetaObject::invokeMethod(&MessageService::instance(), "showWarning",
                                              Qt::QueuedConnection,
                                              Q_ARG(QString, "Error Message"),
                                              Q_ARG(QString, errorMsg));

                    // 發射 forceOff 信號
                    if (viewModel) QMetaObject::invokeMethod(viewModel, "forceOff",
                                              Qt::QueuedConnection,
                                              Q_ARG(TableKind, kind));

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

        // ===== 檢查是否有有效的 DC Load =====
        if (result.dcLoads.isEmpty()) {
            QString errorMsg;
            if (kind == TableKind::Load) {
                errorMsg = "No valid DC Load channel is enabled or configured!";
            } else if (kind == TableKind::DyLoad) {
                errorMsg = "No valid DC Load channel is enabled or configured for dynamic load!";
            } else {
                errorMsg = "No valid DC Load channel is enabled or configured!";
            }

            QMetaObject::invokeMethod(&MessageService::instance(), "showWarning",
                                      Qt::QueuedConnection,
                                      Q_ARG(QString, "Error Message"),
                                      Q_ARG(QString, errorMsg));

            // 發射 forceOff 信號
            if (viewModel) QMetaObject::invokeMethod(viewModel, "forceOff",
                                      Qt::QueuedConnection,
                                      Q_ARG(TableKind, kind));

            for (auto comm : result.commMap) delete comm;
            result.commMap.clear();
            return result;
        }

        // ===== 成功 =====
        result.success = true;

    } catch (const std::exception& ex) {
        // 異常處理：清理所有資源
        for (auto load : result.dcLoads) delete load;
        for (auto comm : result.commMap) delete comm;
        result.dcLoads.clear();
        result.commMap.clear();
        result.success = false;
        qWarning() << "[InstrumentCreator] createDCLoads exception:" << ex.what();
    }

    return result;
}

// ========================================
// Create Relays
// ========================================

InstrumentCreator::RelayResult
InstrumentCreator::createRelays(
    const Page1Config& config,
    QObject* viewModel)
{
    RelayResult result;
    result.success = false;

    QMap<QString, ICommunication*> commMap;
    QVector<RelayBase*> relays;
    QStringList communicationErrors;
    QStringList relayCreationErrors;

    try {
        // ===== 遍歷所有 Relay 儀器 =====
        for (const auto& inst : config.instruments) {
            const QString resource = inst.getResourceString();
            // 只處理啟用的 Relay
            if (inst.type != "Relay" || !inst.enabled) {
                continue;
            }

            // address 為空則靜默跳過，不影響其他 relay
            if (resource.isEmpty()) {
                continue;
            }

            // ===== 步驟 1: 創建或獲取通信對象 =====
            ICommunication* comm = commMap.value(resource, nullptr);

            if (!comm) {
                comm = CommunicationFactory::create(resource);

                if (!comm) {
                    QString error = QString("Failed to create communication for %1 (%2)")
                    .arg(inst.name)
                        .arg(resource);
                    communicationErrors << error;
                    qWarning() << "[InstrumentCreator][Relay]" << error;
                    continue;
                }

                commMap.insert(resource, comm);
            }

            // ===== 步驟 2: 打開通信連接 =====
            if (!comm->isOpen()) {
                if (!comm->open()) {
                    QString error = QString("Failed to open connection for %1 (%2)\nError: %3")
                    .arg(inst.name)
                        .arg(resource)
                        .arg(comm->lastError());
                    communicationErrors << error;
                    qWarning() << "[InstrumentCreator][Relay]" << error;
                    continue;
                }
            }

            // ===== 步驟 3: 創建 Relay 對象 =====
            // 從 address 解析 slave ID（格式：...::SLAVE:N）
            quint8 slaveAddr = 0x01;
            static const QRegularExpression slaveRx("SLAVE:(\\d+)",
                                                    QRegularExpression::CaseInsensitiveOption);
            auto slaveMatch = slaveRx.match(resource);
            if (slaveMatch.hasMatch()) {
                int id = slaveMatch.captured(1).toInt();
                if (id >= 1 && id <= 255)
                    slaveAddr = static_cast<quint8>(id);
            }

            RelayBase* relay = RelayFactory::createRelay(inst.modelName, comm, slaveAddr);

            if (!relay) {
                QString error = QString("Unknown relay model: %1 for %2")
                .arg(inst.modelName)
                    .arg(inst.name);
                relayCreationErrors << error;
                qWarning() << "[InstrumentCreator][Relay]" << error;
                continue;
            }

            relays.append(relay);

            // ===== 步驟 4: 連接 Relay 設備 =====
            if (!relay->isConnected()) {
                relay->connect();

                if (!relay->isConnected()) {
                    QString error = QString("Failed to connect relay %1 (%2)")
                    .arg(inst.name)
                        .arg(resource);
                    communicationErrors << error;
                    qWarning() << "[InstrumentCreator][Relay]" << error;
                    // 不 continue，保留 relay 對象以便稍後清理
                }
            }
        }

        // ===== 步驟 5: 檢查結果 =====
        if (relays.isEmpty()) {
            result.errorMessage = "No relay devices could be created.\n\n";

            if (!communicationErrors.isEmpty()) {
                result.errorMessage += "Communication errors:\n" + communicationErrors.join("\n");
            }
            if (!relayCreationErrors.isEmpty()) {
                result.errorMessage += "\n\nRelay creation errors:\n" + relayCreationErrors.join("\n");
            }

            // 清理已創建的通信對象
            for (auto* c : commMap) {
                if (c) {
                    c->close();
                    delete c;
                }
            }

            return result;
        }

        // 檢查是否有任何錯誤（但仍有部分成功）
        if (!communicationErrors.isEmpty() || !relayCreationErrors.isEmpty()) {
            qWarning() << "[InstrumentCreator][Relay] Some devices failed to initialize:";
            for (const auto& err : communicationErrors) {
                qWarning() << "  -" << err;
            }
            for (const auto& err : relayCreationErrors) {
                qWarning() << "  -" << err;
            }
        }

        // ===== 成功 =====
        result.relays = relays;
        result.commMap = commMap;
        result.success = true;
        return result;

    } catch (const std::exception& e) {
        // 異常處理：清理已創建的資源
        qWarning() << "[InstrumentCreator][Relay] Exception in createRelays:" << e.what();

        for (auto* r : relays) {
            delete r;
        }
        for (auto* c : commMap) {
            if (c) {
                c->close();
                delete c;
            }
        }

        result.errorMessage = QString("Exception: %1").arg(e.what());
        result.success = false;
        return result;
    }
}
