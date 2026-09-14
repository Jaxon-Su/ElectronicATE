#pragma once

#include <QString>
#include <QList>
#include <QStringList>
#include "communicationconfig.h"

struct ChannelSetting {
    QString subModel;
    int     index;
    int     syncType = -1; // -1=unspecified, 0=NONE, 1=MASTER, 2=SLAVE
};

struct InstrumentConfig {
    bool enabled = true;          // checkbox
    QString name;                 // Load1, Source, etc.
    QString type;                 // Load, Relay, InputSource, etc.
    QString modelName;            // 6304, DPO7000, etc.

    // ===== 通訊配置（新增）=====
    CommunicationConfig commConfig;

    // ===== 向後相容：address 字串 =====
    // 讀取時優先使用 commConfig，如果 commConfig 無效則使用 address
    // 寫入時同時寫入兩者以保持相容性
    QString address;

    // 獲取有效的資源字串
    QString getResourceString() const {
        if (commConfig.isValid()) {
            return commConfig.toResourceString();
        }
        return address;
    }

    // 獲取顯示用的地址字串
    QString getDisplayAddress() const {
        if (commConfig.isValid()) {
            return commConfig.toDisplayString();
        }
        return address;
    }

    // 設定地址（同時更新兩者）
    void setAddress(const QString& addr) {
        address = addr;
        // 嘗試從字串解析為 CommunicationConfig
        commConfig = CommunicationConfig::fromResourceString(addr);
    }

    // 設定通訊配置（同時更新兩者）
    void setCommConfig(const CommunicationConfig& cfg) {
        commConfig = cfg;
        address = cfg.toResourceString();
    }

    QList<ChannelSetting> channels;  // 通道設定 (subModel + index)
    QList<int> channelNumbers;       // 實際 channel 編號
};

struct Page1Config {
    int loadOutputs = 1;
    int relayOutputs = 1;
    QList<InstrumentConfig> instruments;
};

struct TableRowInfo {
    QString     instrument;
    QString     type;
    QStringList modelCandidates;  // ex: QStringList{"6304", "6314", "6334A"}
    bool        hasChannels;

    TableRowInfo() = default;
    TableRowInfo(const QString &name, const QString &type, bool channels, const QStringList &models)
        : instrument(name), type(type), hasChannels(channels), modelCandidates(models) {}
};
