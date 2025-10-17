#pragma once
#include "../InstrumentWithCommBase.h"
#include <QString>
#include <QList>

struct StaticCurrentParam {
    //每個load當前Channel，可能有多個電流設定條件。比如L1 L2，有些僅有L1。
    QVector<double> levels;
    QVector<bool> enabledMask;
    double expectedVoltage = 0.0;
};

struct DynamicCurrentParam {
    //每個load當前Channel，可能有多個電流設定條件。比如L1 L2，有些僅有L1。
    QVector<double> levels;
    QVector<double> timings;
    QVector<bool> enabledMask;
    double expectedVoltage = 0.0;
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

    // virtual void setChannel(int) = 0;
    // virtual void setLoadMode(const QString&) = 0;
    // virtual void setVon(double) = 0;
    // virtual void setRiseSlope(double) = 0;
    // virtual void setFallSlope(double) = 0;
    virtual void setStaticCurrent(const StaticCurrentParam&) = 0;
    virtual void setDynamicCurrent(const DynamicCurrentParam&) = 0;


    // 預設實作 可被override
    virtual void setChannel(int) {}
    virtual void setLoadMode(const QString&) {}
    virtual void setVon(double) {}
    virtual void setStaticRiseSlope(double) {} // Static Rise
    virtual void setStaticFallSlope(double) {} // Static fall
    virtual void setDynamicRiseSlope(double) {} // Dynamic Rise
    virtual void setDynamicFallSlope(double) {} // Dynamic fall

    // virtual void setStaticCurrent(double) {} // CC mode
    // virtual void setDynamicCurrent(double) {}

    virtual void setResistance(double) {} // CR mode
    virtual void setVoltage(double) {}    // CV mode

    virtual int getNumSegments() const { return 1; }
    virtual void setChannelIndex(int i)   { m_channelIndex = i; }
    virtual int channelIndex() const      { return m_channelIndex; }

    virtual void setRealChannel(int i)   { m_channel = i; }
    virtual int realChannel() const      { return m_channel; }


private:

    int m_channelIndex = -1; //根據submodel index執行控制，這個是User選用index
    int m_channel =-1;       //實際chroma 硬體 channel

};
