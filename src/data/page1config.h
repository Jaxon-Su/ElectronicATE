#pragma once

#include <QString>
#include <QList>
#include <QStringList>
#include "communicationconfig.h"

struct ChannelSetting {
    QString subModel;
    int index;
    int syncType = -1; // -1=unspecified, 0=NONE, 1=MASTER, 2=SLAVE
};

struct InstrumentConfig {
    bool enabled = true; // checkbox
    QString name;        // Load1, Source, etc.
    QString type;        // Load, Relay, InputSource, etc.
    QString modelName;   // 6304, DPO7000, etc.

    CommunicationConfig commConfig;

    QString getResourceString() const
    {
        return commConfig.isValid() ? commConfig.toResourceString() : QString();
    }

    QString getDisplayAddress() const
    {
        return commConfig.isValid() ? commConfig.toDisplayString() : QString();
    }

    void setCommConfig(const CommunicationConfig& cfg) { commConfig = cfg; }

    QList<ChannelSetting> channels; // 通道設定 (subModel + index)
    QList<int> channelNumbers;      // 實際 channel 編號
};

struct Page1Config {
    int loadOutputs = 1;
    int relayOutputs = 1;
    QList<InstrumentConfig> instruments;
    int dcInputs = 1;
};

struct TableRowInfo {
    QString instrument;
    QString type;
    QStringList modelCandidates; // ex: QStringList{"6304", "6314", "6334A"}
    bool hasChannels;

    TableRowInfo() = default;
    TableRowInfo(const QString& name, const QString& type, bool channels, const QStringList& models)
        : instrument(name), type(type), hasChannels(channels), modelCandidates(models)
    {
    }
};
