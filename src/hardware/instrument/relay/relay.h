#pragma once
#include "instrumentwithcommbase.h"
#include <QVector>

// 繼電器的狀態
enum class RelayStatus {
    Open,       // 斷開
    Closed,     // 閉合
    Unknown     // 未知/讀取失敗
};

class RelayBase : public InstrumentWithCommBase {
public:
    explicit RelayBase(ICommunication* comm = nullptr)
        : InstrumentWithCommBase(comm) {}

    virtual ~RelayBase() = default;

    // 開啟指定通道 (Close the contact)
    virtual bool turnOn(int channel = 1) = 0;

    // 關閉指定通道 (Open the contact)
    virtual bool turnOff(int channel = 1) = 0;

    // 批次控制：同時控制多個通道
    virtual bool setMultiChannels(const QVector<int>& channels, bool on) = 0;

    // 讀取目前繼電器狀態
    virtual RelayStatus getStatus(int channel = 1) = 0;

    // 設定與讀取通道總數
    void setMaxChannels(int count) { m_maxChannels = count; }
    int maxChannels() const { return m_maxChannels; }

    virtual void setChannelIndex(int i)   { m_channelIndex = i; }
    virtual int channelIndex() const      { return m_channelIndex; }

    virtual void setRealChannel(int i)   { m_channel = i; }
    virtual int realChannel() const      { return m_channel; }

protected:
    int m_maxChannels = 1; // 預設為單路

private:

    int m_channelIndex = -1; //根據submodel index執行控制，這個是User選用index
    int m_channel =-1;       //實際硬體 channel
};
