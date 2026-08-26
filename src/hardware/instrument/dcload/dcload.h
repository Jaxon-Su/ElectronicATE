// dcload.h
#pragma once
#include "instrumentwithcommbase.h"
#include <QString>
#include <QList>

struct StaticCurrentParam {
    //每個load當前Channel，可能有多個電流設定條件。比如L1 L2，有些僅有L1。
    QVector<double> levels;
    QVector<bool> enabledMask;
    double expectedVoltage = 0.0;
    QString loadMode;
};

struct DynamicCurrentParam {
    //每個load當前Channel，可能有多個電流設定條件。比如L1 L2，有些僅有L1。
    QVector<double> levels;
    QVector<double> timings;
    QVector<bool> enabledMask;
    double expectedVoltage = 0.0;
    QString loadMode;
};

enum class LoadSyncCapability {
    None,
    MasterSlaveSyncType,  // SYNC:TYPE NONE / MASTER / SLAVE
    ChannelSyncEnable,    // CHAN:SYNC ON / OFF
    ParallelPosition      // Parallel POSITION / TOTAL UNIT
};

// 可擴充的 DC 電子負載抽象父類
class DCLoad : public InstrumentWithCommBase {

public:

    explicit DCLoad(ICommunication* comm = nullptr)
        : InstrumentWithCommBase(comm) {}

    virtual ~DCLoad() = default;

    //必須實作
    virtual void setLoadOn() = 0;
    virtual void setLoadOff() = 0;

    virtual void setStaticCurrent(const StaticCurrentParam&) = 0;
    virtual void setDynamicCurrent(const DynamicCurrentParam&) = 0;

    // 預設實作 可被override
    virtual void setChannel(int) {}
    virtual void setLoadMode(const QString&) {}
    virtual void setVon(double) {}
    virtual void setStaticRiseSlope(double) {}
    virtual void setStaticFallSlope(double) {}
    virtual void setDynamicRiseSlope(double) {}
    virtual void setDynamicFallSlope(double) {}
    virtual void setResistance(double) {}
    virtual void setVoltage(double) {}
    virtual void setCVVoltage(double) {}
    virtual void setCVCurrentLimit(double) {}
    virtual void setCVSettings(double voltage, double currentLimit)
    {
        setCVVoltage(voltage);
        setCVCurrentLimit(currentLimit);
    }
    virtual void setCVSettings(double voltage, double currentLimit, const QString& loadMode)
    {
        const QString requestedMode = loadMode.trimmed().toUpper();
        if (!requestedMode.isEmpty() && requestedMode != "AUTO RANGE" && requestedMode != "NO SETTING")
            setLoadMode(requestedMode);
        setCVSettings(voltage, currentLimit);
    }

    // ── SYNC Dynamic（63600 專用，其他型號預設空實作不受影響）──
    // type: 0=NONE, 1=MASTER, 2=SLAVE
    virtual void setSyncType(int /*type*/) {}
    virtual int syncType() { return -1; }
    virtual bool canQuerySyncType() const { return true; }
    virtual void setConfiguredSyncType(int type) { m_configuredSyncType = type; }
    virtual int configuredSyncType() const { return m_configuredSyncType; }
    virtual int defaultSyncType() const { return m_configuredSyncType; }
    virtual bool shouldApplySyncTypeTransition(int /*type*/) const { return true; }
    virtual LoadSyncCapability syncCapability() const { return LoadSyncCapability::None; }
    virtual QString syncIdentityKey() const { return {}; }
    // MASTER 才需要，告知硬體自己是哪個 channel
    virtual void setSyncChannel(int /*ch*/) {}
    // SYNC:RUN ON/OFF，只對 MASTER channel 呼叫
    virtual void setSyncRun(bool /*on*/) {}

    virtual int getNumSegments() const { return 1; }
    virtual void setChannelIndex(int i)   { m_channelIndex = i; }
    virtual int channelIndex() const      { return m_channelIndex; }

    virtual void setRealChannel(int i)   { m_channel = i; }
    virtual int realChannel() const      { return m_channel; }

private:
    int m_channelIndex = -1; // 根據 submodel index 執行控制，這個是 User 選用 index
    int m_channel = -1;      // 實際 chroma 硬體 channel
    int m_configuredSyncType = -1;
};
