#pragma once
#include "dcload.h"

/**
 * @brief Chroma 63200A 系列高功率電子負載類
 *
 * 支援型號（需在 DCLoadFactory 一併登記）：
 *
 * ── 150V 系列 ─────────────────────────────────────────────
 *   63202A-150-200  :  2kW, 0~150V, 0~200A
 *   63203A-150-300  :  3kW, 0~150V, 0~300A
 *   63204A-150-400  :  4kW, 0~150V, 0~400A
 *   63205A-150-500  :  5kW, 0~150V, 0~500A
 *   63206A-150-600  :  6kW, 0~150V, 0~600A
 *   63208A-150-800  :  8kW, 0~150V, 0~800A
 *
 * ── 600V 系列 ─────────────────────────────────────────────
 *   63202A-600-140  :  2kW, 0~600V, 0~140A
 *   63205A-600-350  :  5kW, 0~600V, 0~350A
 *   63206A-600-420  :  6kW, 0~600V, 0~420A
 *
 * ── 1200V 系列 ────────────────────────────────────────────
 *   63202A-1200-80  :  2kW, 0~1200V, 0~80A
 *   63204A-1200-160 :  4kW, 0~1200V, 0~160A
 *   63205A-1200-200 :  5kW, 0~1200V, 0~200A
 *   63206A-1200-240 :  6kW, 0~1200V, 0~240A
 *   63208A-1200-320 :  8kW, 0~1200V, 0~320A
 *
 * 與 63600 系列的主要差異：
 *   - 63200A 為單通道整機，無 CHAN 切換指令
 *   - 功率等級更高（2kW~24kW），電流規格以三擋對應 1/10, 1/2, 全量程
 *   - Von 控制指令相同（CONF:VOLT:ON）
 *   - 動態模式 T1/T2 解析度 1μs，範圍 10μs~99999.999ms
 */
class Chroma63200A : public DCLoad
{
public:
    /**
     * @param subModel 型號字串，例如 "63205A-150-500"
     * @param comm     通訊物件
     */
    Chroma63200A(const QString& subModel, ICommunication* comm = nullptr)
        : DCLoad(comm), m_subModel(subModel) {}

    ~Chroma63200A() override;

    // ==================== DCLoad 必要實作 ====================
    void setLoadOn()  override;
    void setLoadOff() override;

    /**
     * 63200A 每台為單通道，L1/L2 為同一通道的雙電流位準，
     * 與 63600 相同返回 2（L1/L2）。
     */
    int getNumSegments() const override { return 2; }

    void setChannel(int channel) override;
    void setSyncType(int type) override;
    int syncType() override;
    LoadSyncCapability syncCapability() const override { return LoadSyncCapability::MasterSlaveSyncType; }
    QString syncIdentityKey() const override { return QString("SYNC:%1").arg(getaddress()); }
    void setSyncChannel(int ch) override;
    void setSyncRun(bool on) override;

    void setLoadMode(const QString& mode) override;
    void setVon(double von)               override;
    void setStaticRiseSlope(double slope) override;
    void setStaticFallSlope(double slope) override;
    void setDynamicRiseSlope(double slope) override;
    void setDynamicFallSlope(double slope) override;
    void setStaticCurrent(const StaticCurrentParam& param)   override;
    void setDynamicCurrent(const DynamicCurrentParam& param) override;
    void setCVVoltage(double voltage) override;
    void setCVCurrentLimit(double currentLimit) override;
    void setCVSettings(double voltage, double currentLimit) override;
    void setCVSettings(double voltage, double currentLimit, const QString& loadMode) override;

    QString model()    const override;
    QString vendor()   const override;
    QString subModel() const { return m_subModel; }

private:
    QString m_subModel;
};
