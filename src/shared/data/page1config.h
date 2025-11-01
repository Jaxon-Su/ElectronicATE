#pragma once

#include <QString>
#include <QList>
#include <QComboBox>

struct ChannelSetting {
    QString subModel;
    int     index;
};

struct InstrumentConfig {
    bool enabled; //checkbox
    QString name;
    QString type;
    QString modelName;
    QString address;
    QList<ChannelSetting> channels; // 裡面有記錄 channels index(用戶選擇)，非當前channel
    QList<int> channelNumbers;      //實際channel
};

struct Page1Config {
    int loadOutputs;
    int relayOutputs;
    QList<InstrumentConfig> instruments;
};

struct TableRowInfo {
    QString     instrument;
    QString     type;
    QStringList modelCandidates; // ex:QStringList{"6304", "6314", "6334A"}
    bool        hasChannels;

    TableRowInfo() = default;
    TableRowInfo(const QString &name, const QString &type, bool channels, const QStringList &models)
        : instrument(name), type(type), hasChannels(channels), modelCandidates(models) {}
};
