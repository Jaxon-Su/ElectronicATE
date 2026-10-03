#include "instrumentcreator.h"
#include "acsource.h"
#include "dcload.h"
#include "relay.h"
#include "icommunication.h"
#include "communicationfactory.h"
#include "acsourcefactory.h"
#include "dcloadfactory.h"
#include "relayfactory.h"
#include <memory>
#include <QScopeGuard>
#include <QRegularExpression>
#include <QDebug>

namespace
{
void releaseConstruction(QVector<DCLoad *> &drivers, QMap<QString, ICommunication *> &communications)
{
    ResourceCleaner::cleanupDCLoads(drivers, communications);
}
void releaseConstruction(QVector<RelayBase *> &drivers, QMap<QString, ICommunication *> &communications)
{
    ResourceCleaner::cleanupRelays(drivers, communications);
}
} // namespace

// Create AC Source

InstrumentCreator::ACSourceResult InstrumentCreator::createACSource(const Page1Config &config)
{
    for (const auto &instrument : config.instruments) {
        if (instrument.name != "ACSource" || instrument.type != "InputSource")
            continue;
        if (!instrument.enabled)
            return {nullptr, nullptr, false, "AC source is disabled"};
        const QString resource = instrument.getResourceString();
        if (instrument.modelName.isEmpty() || resource.isEmpty())
            return {nullptr, nullptr, false, "AC source model or address is missing"};
        auto communication = std::unique_ptr<ICommunication>(CommunicationFactory::create(resource));
        if (!communication)
            return {nullptr, nullptr, false, "Invalid AC source communication resource"};
        auto source = std::unique_ptr<ACSource>(
            ACSourceFactory::createACSource(instrument.modelName, communication.get()));
        if (!source)
            return {nullptr, nullptr, false, "Unsupported AC source model"};
        source->connect();
        if (!source->isConnected())
            return {nullptr, nullptr, false, source->lastError()};
        return {source.release(), communication.release(), true, {}};
    }
    return {nullptr, nullptr, false, "No AC source configured"};
}

// Create DC Loads

InstrumentCreator::DCLoadResult InstrumentCreator::createDCLoads(const Page1Config &config, TableKind kind)
{
    DCLoadResult result;

    try {
        // 遍歷所有儀器配置
        for (const auto &ic : config.instruments) {
            const QString resource = ic.getResourceString();
            // 過濾條件：必須是啟用的 Load 類型
            if (!ic.enabled || ic.type != "Load")
                continue;
            if (ic.modelName.isEmpty() || resource.isEmpty())
                continue;

            // 獲取或創建通信對象
            ICommunication *comm = result.m_commMap.value(resource, nullptr);
            if (!comm) {
                comm = CommunicationFactory::create(resource);
                if (!comm)
                    continue;
                result.m_commMap[resource] = comm;
            }

            // 為每個通道創建 DC Load
            for (int i = 0; i < ic.channels.size(); ++i) {
                const auto &ch = ic.channels[i];
                if (ch.subModel.isEmpty() || ch.index <= 0)
                    continue;

                // 創建 DC Load
                // qDebug() << "ch.subModel" << ch.subModel;
                auto ownedLoad = std::unique_ptr<DCLoad>(DCLoadFactory::createDCLoad(ch.subModel, comm));
                DCLoad *dcLoad = ownedLoad.get();
                if (!dcLoad)
                    continue;

                // 設置通道參數
                int uiIndex = ch.index;
                int hwChannel = ic.channelNumbers.value(i, -1);
                dcLoad->setRealChannel(hwChannel);
                dcLoad->setChannelIndex(uiIndex);
                dcLoad->setAddress(resource);
                dcLoad->setConfiguredSyncType(ch.syncType);

                qDebug() << "[InstrumentCreator] DCLoad created:" << ic.name << "slot" << i
                         << "subModel=" << ch.subModel << "| CHAN(realChannel)=" << hwChannel
                         << "| outputIndex(channelIndex)=" << uiIndex
                         << "| configuredSyncType=" << ch.syncType;

                // 連接檢查
                dcLoad->connect();
                if (!dcLoad->isConnected()) {
                    result.errorMessage = dcLoad->model() + " communication open failed!";

                    ownedLoad.reset();

                    releaseConstruction(result.m_dcLoads, result.m_commMap);
                    return result;
                }

                result.m_dcLoads.append(ownedLoad.release());
            }
        }

        // 檢查是否有有效的 DC Load
        if (result.m_dcLoads.isEmpty()) {
            QString errorMsg;
            if (kind == TableKind::Load) {
                errorMsg = "No valid DC Load channel is enabled or configured!";
            } else if (kind == TableKind::DyLoad) {
                errorMsg = "No valid DC Load channel is enabled or configured for dynamic load!";
            } else {
                errorMsg = "No valid DC Load channel is enabled or configured!";
            }

            result.errorMessage = errorMsg;
            releaseConstruction(result.m_dcLoads, result.m_commMap);
            return result;
        }

        // 成功
        result.success = true;

    } catch (const std::exception &ex) {
        result.success = false;
        result.errorMessage = QString::fromUtf8(ex.what());
    } catch (...) {
        result.success = false;
        result.errorMessage = "Unexpected DC load construction error";
    }

    if (!result.success)
        releaseConstruction(result.m_dcLoads, result.m_commMap);
    return result;
}

// Create Relays

InstrumentCreator::RelayResult InstrumentCreator::createRelays(const Page1Config &config)
{
    RelayResult result;
    result.success = false;

    QMap<QString, ICommunication *> commMap;
    QVector<RelayBase *> relays;
    bool transferred = false;
    const auto rollback = qScopeGuard([&] {
        if (!transferred)
            releaseConstruction(relays, commMap);
    });
    QStringList communicationErrors;
    QStringList relayCreationErrors;

    try {
        // 遍歷所有 Relay 儀器
        for (const auto &inst : config.instruments) {
            const QString resource = inst.getResourceString();
            // 只處理啟用的 Relay
            if (inst.type != "Relay" || !inst.enabled) {
                continue;
            }

            // address 為空則靜默跳過，不影響其他 relay
            if (resource.isEmpty()) {
                continue;
            }

            // 步驟 1: 創建或獲取通信對象
            ICommunication *comm = commMap.value(resource, nullptr);

            if (!comm) {
                comm = CommunicationFactory::create(resource);

                if (!comm) {
                    QString error =
                        QString("Failed to create communication for %1 (%2)").arg(inst.name).arg(resource);
                    communicationErrors << error;
                    qWarning() << "[InstrumentCreator][Relay]" << error;
                    continue;
                }

                commMap.insert(resource, comm);
            }

            // 步驟 2: 打開通信連接
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

            // 步驟 3: 創建 Relay 對象
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

            RelayBase *relay = RelayFactory::createRelay(inst.modelName, comm, slaveAddr);

            if (!relay) {
                QString error = QString("Unknown relay model: %1 for %2").arg(inst.modelName).arg(inst.name);
                relayCreationErrors << error;
                qWarning() << "[InstrumentCreator][Relay]" << error;
                continue;
            }

            relays.append(relay);
            result.m_configurations.append(inst);

            // 步驟 4: 連接 Relay 設備
            if (!relay->isConnected()) {
                relay->connect();

                if (!relay->isConnected()) {
                    QString error = QString("Failed to connect relay %1 (%2)").arg(inst.name).arg(resource);
                    communicationErrors << error;
                    qWarning() << "[InstrumentCreator][Relay]" << error;
                    // 不 continue，保留 relay 對象以便稍後清理
                }
            }
        }

        // 步驟 5: 檢查結果
        if (relays.isEmpty()) {
            result.errorMessage = "No relay devices could be created.\n\n";

            if (!communicationErrors.isEmpty()) {
                result.errorMessage += "Communication errors:\n" + communicationErrors.join("\n");
            }
            if (!relayCreationErrors.isEmpty()) {
                result.errorMessage += "\n\nRelay creation errors:\n" + relayCreationErrors.join("\n");
            }

            return result;
        }

        // 檢查是否有任何錯誤（但仍有部分成功）
        if (!communicationErrors.isEmpty() || !relayCreationErrors.isEmpty()) {
            qWarning() << "[InstrumentCreator][Relay] Some devices failed to initialize:";
            for (const auto &err : communicationErrors) {
                qWarning() << "  -" << err;
            }
            for (const auto &err : relayCreationErrors) {
                qWarning() << "  -" << err;
            }
        }

        // 成功
        result.m_relays = relays;
        result.m_commMap = commMap;
        result.errorMessage = (communicationErrors + relayCreationErrors).join("\n");
        result.success = true;
        transferred = true;
        return result;

    } catch (const std::exception &e) {
        // 異常處理：清理已創建的資源
        qWarning() << "[InstrumentCreator][Relay] Exception in createRelays:" << e.what();

        result.m_relays.clear();
        result.m_commMap.clear();
        result.errorMessage = QString("Exception: %1").arg(e.what());
        result.success = false;
        return result;
    } catch (...) {
        result.m_relays.clear();
        result.m_commMap.clear();
        result.success = false;
        result.errorMessage = "Unexpected relay construction error";
        return result;
    }
}
