// chroma63600.h
#pragma once
#include "dcload.h"
#include <QVector>
#include <QList>

/**
 * @brief Chroma 63600 系列電子負載類
 *
 * 支援型號：
 * - 63610-80-20:  100Wx2 (雙通道), 0~80V,  0~20A
 * - 63630-80-60:  300W,  0~80V,  0~60A
 * - 63640-80-80:  400W,  0~80V,  0~80A
 * - 63640-150-60: 400W,  0~150V, 0~60A
 * - 63630-600-15: 600W,  0~600V, 0~15A
 */
class Chroma63600 : public DCLoad
{
public:
    Chroma63600(const QString& subModel, ICommunication* comm = nullptr)
        : DCLoad(comm), m_model(subModel) {}

    ~Chroma63600() override;

    // ==================== DCLoad 必要實作 ====================
    void setLoadOn() override;
    void setLoadOff() override;

    int getNumSegments() const override { return 2; }  // 63600 支援 L1/L2

    void setChannel(int channel) override;
    void setLoadMode(const QString& mode) override;
    void setVon(double von) override;
    void setStaticRiseSlope(double slope) override;
    void setStaticFallSlope(double slope) override;
    void setDynamicRiseSlope(double slope) override;
    void setDynamicFallSlope(double slope) override;
    void setStaticCurrent(const StaticCurrentParam&) override;
    void setDynamicCurrent(const DynamicCurrentParam&) override;

    QString model() const override;
    QString vendor() const override;

    QString subModel() const { return m_model; }

    // ==================== 63600 特有功能 ====================
    void setCurrentRange(const QString& range);
    void setVoltageRange(const QString& range);
    void setPower(double power);

    // ==================== SYNC Dynamic ====================
    // type: 0=NONE, 1=MASTER, 2=SLAVE
    // 呼叫前須先 setChannel(realChannel())
    void setSyncType(int type) override;
    int syncType() override;
    bool canQuerySyncType() const override { return false; }
    int defaultSyncType() const override;
    bool shouldApplySyncTypeTransition(int /*type*/) const override { return true; }
    LoadSyncCapability syncCapability() const override { return LoadSyncCapability::MasterSlaveSyncType; }
    QString syncIdentityKey() const override { return QString("SYNC:%1:CHAN:%2").arg(getaddress()).arg(realChannel()); }

    // 只有 MASTER 需要呼叫，ch = realChannel()
    void setSyncChannel(int ch) override;

    // 只對 MASTER 呼叫，觸發全部 channel 同步啟動/停止
    void setSyncRun(bool on) override;

private:
    QString m_model;
};
