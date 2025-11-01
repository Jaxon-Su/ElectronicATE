#pragma once
#include "dcload.h"
#include <QVector>
#include <QList>

// enum class ChannelType {MAX,MIN};
// enum class VoltageType {V,mV};
// enum class VoltageRangeType {H,L};
// enum class AutoMode {LOAD,PROGRAM};
// enum class ConfigLoad {UPDATED,OLD};
// enum class CurrentStaticMode {L1,L2};
// enum class CurrentStaticType {MAX,MIN};
// enum class StaticRiseFallMode {RISE, FALL};
// enum class StaticRiseFallType {MAX, MIN};
// enum class CurrentDynamicMode {L1,L2};
// enum class CurrentDynamicType {MAX,MIN};
// enum class DynamicRiseFallMode {RISE, FALL};
// enum class DynamicRiseFallType {MAX,MIN};
// enum class DynamicTimeMode {T1,T2};
// enum class DynamicTimeType {MAX,MIN};
// enum class DynamicTime {S,mS};
// enum class LoadState { ON, OFF };
// enum class ShortKeyMode { TOGGLE, HOLD };
// enum class MeasureInputType { UUT, LOAD };
// enum class LoadMode { CCL, CCH, CCDL, CCDH, CRL, CRH, CV};
// enum class ProgramMode { SKIP, AUTO, MANUAL, CHAR };
// enum class ResistanceChannel { L1, L2 };
// enum class ResistanceSlope { RISE, FALL };
// enum class ResistanceType { MAX, MIN };
// enum class ProgramTimeType { None, NR2 };
// enum class ProgramTimeMaxMin { None, MAX, MIN };
// enum class ShowDisplayType { L, R, LRV, LRI };
// enum class VoltageChannel { L1, L2 };
// enum class VoltageQueryType { None, MAX, MIN };
// enum class VoltageMode { FAST, SLOW };

class Chroma6310 : public DCLoad
{
public:
    Chroma6310(const QString& subModel,ICommunication* comm = nullptr)
        : DCLoad(comm), m_model(subModel){};

    ~Chroma6310() override;

    // override DCLoad class----------------------------

    void setLoadOn() override;
    void setLoadOff() override;

    int getNumSegments() const override { return 2; }  // Chroma 6310 支援 L1/L2

    void setChannel(int channel) override;
    void setLoadMode(const QString& mode) override;
    void setVon(double von) override;
    void setStaticRiseSlope(double slope) override;
    void setStaticFallSlope(double slope) override;
    void setDynamicRiseSlope(double slope) override;
    void setDynamicFallSlope(double slope) override;
    void setStaticCurrent(const StaticCurrentParam&) override; //CCL CCH根據電流大小選用 精度問題
    void setDynamicCurrent(const DynamicCurrentParam&) override;


    QString model() const override;
    QString vendor() const override;

    QString subModel() const { return m_model; }

    QString m_model;


    // // --- Common----------------------------------------
    // void reSet();
    // QString queryIDN();
    // // --- channel---
    // void selectChannel(int channel);
    // void selectChannel(ChannelType type);
    // int queryChannel();
    // int queryChannel(ChannelType type);

    // void acTive(bool on);
    // void syncOn(bool on);
    // int querySyncOn();
    // QString chanID();

    // // --- CONFIGURE---
    // void setVon(VoltageType type , double value);
    // double queryVon();

    // void setVoltRange(double voltage);
    // void setVoltRange(VoltageRangeType type);
    // double queryVoltRange();
    // void setVoltLatch(bool on);
    // int queryVoltLatch();
    // void setAutoLoad(bool on);
    // int queryAutoLoad();
    // void setAutoMode(AutoMode mode);
    // int queryAutoMode();
    // void setConfigureSound(bool on);
    // int queryConfigureSound();
    // void setRemote(bool on);
    // void ConfigureSave();
    // void setConfigureLoad(ConfigLoad mode);
    // int queryConfigureLoad();

    // // // --- CURRENT---
    // void currStaticL1L2(CurrentStaticMode mode , double value);
    // void currStaticL1L2(CurrentStaticMode mode , CurrentStaticType type);
    // double querycurrStaticL1L2(CurrentStaticMode mode);
    // double querycurrStaticL1L2(CurrentStaticMode mode,CurrentStaticType type);
    // void currStaticRiseFall(double value);
    // double queryStaticRiseFall(StaticRiseFallMode mode);
    // double queryStaticRiseFall(StaticRiseFallMode mode,StaticRiseFallType type);

    // void currDynamicL1L2(CurrentDynamicMode mode , double value);
    // void currDynamicL1L2(CurrentDynamicMode mode , CurrentDynamicType type);
    // double querycurrDynamicL1L2(CurrentDynamicMode mode);
    // double querycurrDynamicL1L2(CurrentDynamicMode mode,CurrentDynamicType type);
    // void currDynamicRiseFall(double value);
    // void currDynamicRiseFall(DynamicRiseFallMode mode,double value);
    // double querycurrDynamicRiseFall(DynamicRiseFallMode mode);
    // double querycurrDynamicRiseFall(DynamicRiseFallMode mode,DynamicRiseFallType type);

    // void currDynamicT1T2(DynamicTimeMode mode,DynamicTime type);
    // void currDynamicT1T2(DynamicTimeMode mode,DynamicTimeType type);
    // double querycurrDynamicT1T2(DynamicTimeMode mode);
    // double querycurrDynamicT1T2(DynamicTimeMode mode,DynamicTimeType type);

    // // // --- FETCH Subsystem---
    // double fetchVoltage();
    // double fetchCurrent();
    // int fetchStatus();
    // QVector<double> fetchAllVoltage();
    // QVector<double> fetchAllCurrent();

    // // // --- LOAD Subsystem---
    // void loadState(bool on); // Load ON OFF
    // int loadState();
    // void shortState(bool on);
    // int shortState();
    // void shortLoadKey(ShortKeyMode mode);
    // void clearLoadProtection();
    // void clearLoad();
    // void saveLoad();

    // // // --- MEASURE Subsystem ---
    // double measureVoltage();
    // double measureCurrent();
    // int measureInput();
    // void measureInput(MeasureInputType type);
    // int measureScan();
    // void measureScan(bool on);
    // QVector<double> measureAllVoltage();
    // QVector<double> measureAllCurrent();

    // // // --- MODE Subsystem ---
    // void setMode(LoadMode mode);
    // QString queryloadMode();

    // // --- PROGRAM Subsystem ---
    // void setProgramFile(int index);
    // int queryProgramFile();
    // void setProgramSequence(int index);
    // int queryProgramSequence();
    // void setProgramShort(int channel, int value);
    // int queryProgramShort(int channel);
    // void setProgramShortTime(int channel, double value);
    // double queryProgramShortTime(int channel);
    // void setProgramMode(ProgramMode mode);
    // ProgramMode queryProgramMode();
    // void setProgramActive(int value);
    // int queryProgramActive();
    // void setProgramChain(int value);
    // int queryProgramChain();
    // void setProgramPETime(int value);
    // void setProgramONTime(int value);
    // void setProgramOFFTime(int value);
    // double queryProgramPFDTime();
    // double queryProgramONTime();
    // double queryProgramOFFTime();
    // void programRun();
    // void programRun(bool on);  // ON1/OFF0
    // void programSave();

    // //--- RESISTANCE Subsystem ---
    // void setResistance(ResistanceChannel mode, double value);
    // void setResistance(ResistanceSlope mode, double value);
    // double queryResistance(ResistanceChannel mode);
    // double queryResistance(ResistanceChannel mode, ResistanceType type);
    // double queryResistance(ResistanceSlope mode);
    // double queryResistance(ResistanceSlope mode, ResistanceType type);

    // // // --- RUN Subsystem ---
    // void setRun();

    // // --- SHOW Subsystem ---
    // void showDisplay(ShowDisplayType type);

    // //--- VOLTAGE Subsystem ---
    // void setVoltage(VoltageChannel ch, double value);
    // double queryVoltage(VoltageChannel ch, VoltageQueryType type = VoltageQueryType::None);
    // void setVoltageCurrent(VoltageChannel ch, double value);
    // double queryVoltageCurrent(VoltageChannel ch, VoltageQueryType type = VoltageQueryType::None);
    // void setVoltageMode(VoltageMode mode);
    // VoltageMode queryVoltageMode();

};

