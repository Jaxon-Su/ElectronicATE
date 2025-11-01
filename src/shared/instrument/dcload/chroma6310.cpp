#include "chroma6310.h"
#include <QDebug>
#include "chromaload6310spec.h"
#include <optional>

// Chroma6310::Chroma6310(ICommunication* comm) : DCLoad(comm) {}
Chroma6310::~Chroma6310() { disconnect(); }

// DCLoad override---------------------------------------------------------
void Chroma6310::setLoadOn() {
    //Load ON
    // qDebug() << QString("Load ON");
    sendCommandWithLog(QString("LOAD ON"), "[Chroma6310]");
}

void Chroma6310::setLoadOff() {
    //Load OFF
    // qDebug() << QString("Load OFF");
    sendCommandWithLog(QString("LOAD OFF"), "[Chroma6310]");
}

void Chroma6310::setChannel(int channel) {
    QString cmd = QString("CHAN %1").arg(channel);
    write(cmd);
}

void Chroma6310::setLoadMode(const QString& mode) {
    static const QSet<QString> validModes = {
        "CCL", "CCH", "CCDL", "CCDH", "CRL", "CRH", "CV"
    };
    if (!validModes.contains(mode)) {
        qWarning() << "Chroma6310::setLoadMode: Invalid mode:" << mode;
        return;
    }
    QString cmd = QString("MODE %1").arg(mode);
    write(cmd);
}

// 設定 Von，單位V
void Chroma6310::setVon(double von)
{
    // 根據 Chroma 指令，單位需自行處理（這裡假設以 V 為主）
    QString cmd = QString("CONF:VOLT:ON %1").arg(von);
    write(cmd);
}

// 設定靜態模式電流的 Rise Slope，單位A/us（依手冊）
void Chroma6310::setStaticRiseSlope(double slope)
{
    QString cmd = QString("CURR:STAT:RISE %1").arg(slope);
    write(cmd);
}

// 設定靜態模式電流的 Fall Slope，單位A/us（依手冊）
void Chroma6310::setStaticFallSlope(double slope)
{
    QString cmd = QString("CURR:STAT:FALL %1").arg(slope);
    write(cmd);
}

// 設定動態模式電流的 Rise Slope，單位A/us（依手冊）
void Chroma6310::setDynamicRiseSlope(double slope)
{
    QString cmd = QString("CURR:DYN:RISE %1").arg(slope);
    write(cmd);
}

// 設定動態模式電流的 Fall Slope，單位A/us（依手冊）
void Chroma6310::setDynamicFallSlope(double slope)
{
    QString cmd = QString("CURR:DYN:FALL %1").arg(slope);
    write(cmd);
}

void Chroma6310::setStaticCurrent(const StaticCurrentParam& param)
{
    if (param.levels.isEmpty()) return;

    double current = param.levels[0];
    double voltage = param.expectedVoltage;

    // 選擇模式
    QString modeStr = selectOptimalLoadMode(m_model, current, voltage);

    setLoadMode(modeStr);

    // 設定電流
    int segs = getNumSegments();
    for (int i = 0; i < segs; ++i) {
        bool enable = param.enabledMask.isEmpty() ||
                      (i < param.enabledMask.size() && param.enabledMask[i]);
        if (!enable) continue;
        if (i >= param.levels.size()) continue;

        double value = param.levels[i];
        QString segName = (i == 0) ? "L1" : "L2";
        QString cmd = QString("CURR:STAT:%1 %2").arg(segName).arg(value);
        sendCommandWithLog(cmd, "[Chroma6310] setStaticCurrent:" + segName);
    }

    if (param.levels.size() > segs) {
        qWarning() << "[Chroma6310] Only L1/L2 supported, ignore L3+";
    }
}

void Chroma6310::setDynamicCurrent(const DynamicCurrentParam& param)
{
    if (param.levels.isEmpty()) return;

    double maxCurrent = *std::max_element(param.levels.begin(), param.levels.end());
    double voltage = param.expectedVoltage;

    // 選擇動態模式
    QString baseMode = selectOptimalLoadMode(m_model, maxCurrent, voltage);

    // 轉換為動態模式
    QString modeStr = (baseMode == "CCH") ? "CCDH" : "CCDL";

    setLoadMode(modeStr);

    // 設定動態電流級別 (L1, L2)
    int segs = getNumSegments();
    for (int i = 0; i < segs && i < param.levels.size(); ++i) {
        bool enable = param.enabledMask.isEmpty() ||
                      (i < param.enabledMask.size() && param.enabledMask[i]);
        if (!enable) continue;

        double value = param.levels[i];
        QString segName = (i == 0) ? "L1" : "L2";
        QString cmd = QString("CURR:DYN:%1 %2").arg(segName).arg(value);
        sendCommandWithLog(cmd, "[Chroma6310] setDynamicCurrent:" + segName);
    }

    // 設定動態時間參數 (T1, T2)
    if (!param.timings.isEmpty()) {
        if (param.timings.size() >= 1) {
            QString cmd = QString("CURR:DYN:T1 %1").arg(param.timings[0]);
            sendCommandWithLog(cmd, "[Chroma6310] setDynamicCurrent:T1");
        }
        if (param.timings.size() >= 2) {
            QString cmd = QString("CURR:DYN:T2 %1").arg(param.timings[1]);
            sendCommandWithLog(cmd, "[Chroma6310] setDynamicCurrent:T2");
        }
    }

    if (param.levels.size() > segs) {
        qWarning() << "[Chroma6310] Only L1/L2 supported for dynamic mode, ignore L3+";
    }
}

QString Chroma6310::model() const {
    return "6310";
}
QString Chroma6310::vendor() const {
    return "Chroma";
}

// void Chroma6310::reSet() {
//     QString cmd = QString("*RST");
//     write(cmd);
// }

// QString Chroma6310::queryIDN() {
//     QString result;
//     if (queryString("IDN?", result))
//         return result;
//     else
//         return QString();
// }

// void Chroma6310::selectChannel(int channel) {
//     QString cmd = QString("CHAN %1").arg(channel);
//     write(cmd);
// }

// void Chroma6310::selectChannel(ChannelType type) {
//     QString key;
//     switch (type) {
//     case ChannelType::MAX: key = "MAX"; break;
//     case ChannelType::MIN: key = "MIN"; break;
//     default: key = ""; break;
//     }
//     QString cmd = QString("CHAN %1").arg(key);
//     write(cmd);
// }

// int Chroma6310::queryChannel() {
//     int value = -1;
//     if (queryInt("CHAN?", value))
//         return value;
//     else
//         return -1;
// }

// int Chroma6310::queryChannel(ChannelType type) {
//     int value = -1;
//     QString key;
//     switch (type) {
//     case ChannelType::MAX: key = "MAX"; break;
//     case ChannelType::MIN: key = "MIN"; break;
//     default: key = ""; break;
//     }
//     QString cmd = QString("CHAN? %1").arg(key);
//     if (queryInt(cmd, value))
//         return value;
//     else
//         return -1;
// }

// void Chroma6310::acTive(bool on) {
//     QString cmd = QString("ACTive %1").arg(on ? "ON" : "OFF");
//     write(cmd);
// }

// void Chroma6310::syncOn(bool on) {
//     QString cmd = QString("CHAN:SYNC %1").arg(on ? "ON" : "OFF");
//     write(cmd);
// }

// QString Chroma6310::chanID() {
//     QString result;
//     if (queryString("ID?", result))
//         return result;
//     else
//         return QString();
// }


// int Chroma6310::querySyncOn(){
//     int  value= -1;
//     if (queryInt("CHAN:SYNC?", value))
//         return value;
//     else
//         return -1;
// }

// void Chroma6310::setVon(VoltageType type, double value) {
//     QString key;
//     switch (type) {
//     case VoltageType::V:  key = "";    break;  // 預設單位為 V，指令不用特別加
//     case VoltageType::mV: key = "mV";  break;
//     default:              key = "";    break;
//     }
//     // 組合 SCPI 指令，mV 時會變成 300mV，V 時只有 1
//     QString cmd = QString("CONF:VOLT:ON %1%2").arg(value).arg(key);
//     write(cmd);
// }

// double Chroma6310::queryVon(){
//     double  value= -1;
//     if (queryDouble("CONF:VOLT:ON?", value))
//         return value;
//     else
//         return -1;
// }

// void Chroma6310::setVoltRange(double voltage) {
//     QString cmd = QString("CONF:VOLT:RANG %1").arg(voltage);
//     write(cmd);
// }

// void Chroma6310::setVoltRange(VoltageRangeType type) {
//     QString key;
//     switch (type) {
//     case VoltageRangeType::H: key = "H"; break;
//     case VoltageRangeType::L: key = "L"; break;
//     default: key = ""; break;
//     }
//     QString cmd = QString("CONF:VOLT:RANG %1").arg(key);
//     write(cmd);
// }

// double Chroma6310::queryVoltRange() {
//     double  value= -1;
//     if (queryDouble("CONF:VOLT:RANG?", value))
//         return value;
//     else
//         return -1;
// }

// void Chroma6310::setVoltLatch(bool on) {
//     QString cmd = QString("CONF:VOLT:LATC %1").arg(on ? "ON" : "OFF");
//     write(cmd);
// }

// int Chroma6310::queryVoltLatch(){
//     int  value= -1;
//     if (queryInt("CONF:VOLT:LATC?", value))
//         return value;
//     else
//         return -1;
// }

// void Chroma6310::setAutoLoad(bool on) {
//     QString cmd = QString("CONF:AUTO:LOAD %1").arg(on ? "ON" : "OFF");
//     write(cmd);
// }

// int Chroma6310::queryAutoLoad(){
//     int  value= -1;
//     if (queryInt("CONF:AUTO:LOAD?", value))
//         return value;
//     else
//         return -1;
// }

// void Chroma6310::setAutoMode(AutoMode mode){
//     QString key;
//     switch (mode) {
//     case AutoMode::LOAD: key = "LOAD"; break;
//     case AutoMode::PROGRAM: key = "PROGRAM"; break;
//     default: key = ""; break;
//     }
//     QString cmd = QString("CONF:AUTO:MODE %1").arg(key);
//     write(cmd);
// }

// int Chroma6310::queryAutoMode(){
//     int  value= -1;
//     if (queryInt("CONF:AUTO:MODE?", value))
//         return value;
//     else
//         return -1;
// }

// void Chroma6310::setConfigureSound(bool on) {
//     QString cmd = QString("CONFigure:SOUND %1").arg(on ? "ON" : "OFF");
//     write(cmd);
// }

// int Chroma6310::queryConfigureSound(){
//     int  value= -1;
//     if (queryInt("CONF:SOUND?", value))
//         return value;
//     else
//         return -1;
// }

// void Chroma6310::setRemote(bool on) {
//     QString cmd = QString("CONFigure:REMote %1").arg(on ? "ON" : "OFF");
//     write(cmd);
// }

// void Chroma6310::ConfigureSave() {
//     QString cmd = QString("CONF:SAVE");
//     write(cmd);
// }

// void Chroma6310::setConfigureLoad(ConfigLoad mode) {
//     QString key;
//     switch (mode) {
//     case ConfigLoad::UPDATED: key = "UPDATED"; break;
//     case ConfigLoad::OLD: key = "OLD"; break;
//     default: key = ""; break;
//     }
//     QString cmd = QString("CONF:AUTO:MODE %1").arg(key);
//     write(cmd);
// }

// int Chroma6310::queryConfigureLoad(){
//     int  value= -1;
//     if (queryInt("CONF:AUTO:MODE?", value))
//         return value;
//     else
//         return -1;
// }

// void Chroma6310::currStaticL1L2(CurrentStaticMode mode , double value) {
//     QString key;
//     switch (mode) {
//     case CurrentStaticMode::L1:  key = "L1";    break;  // 預設單位為 V，指令不用特別加
//     case CurrentStaticMode::L2: key = "L2";  break;
//     default:              key = "";    break;
//     }
//     QString cmd = QString("CURR:STAT:%1 %2").arg(key).arg(value);
//     write(cmd);
// }

// void Chroma6310::currStaticL1L2(CurrentStaticMode mode , CurrentStaticType type) {
//     QString key;
//     switch (mode) {
//     case CurrentStaticMode::L1:  key = "L1";    break;
//     case CurrentStaticMode::L2:  key = "L2";    break;
//     default:                 key = "";      break;
//     }
//     QString key2;
//     switch (type) {
//     case CurrentStaticType::MAX:  key2 = "MAX";    break;
//     case CurrentStaticType::MIN:  key2 = "MIN";    break;
//     default:                      key2 = "";       break;
//     }
//     QString cmd = QString("CURR:STAT:%1 %2").arg(key,key2);
//     write(cmd);
// }

// double Chroma6310::querycurrStaticL1L2(CurrentStaticMode mode){
//     double value = -1;
//     QString key;
//     switch (mode) {
//     case CurrentStaticMode::L1: key = "L1"; break;
//     case CurrentStaticMode::L2: key = "L2"; break;
//     default: key = ""; break;
//     }
//     QString cmd = QString("CURRent:STATic:%1?").arg(key);
//     if (queryDouble(cmd ,value))
//         return value;
//     else
//         return -1;
// }

// double Chroma6310::querycurrStaticL1L2(CurrentStaticMode mode,CurrentStaticType type){
//     double value = -1;
//     QString key;
//     switch (mode) {
//     case CurrentStaticMode::L1: key = "L1"; break;
//     case CurrentStaticMode::L2: key = "L2"; break;
//     default: key = ""; break;
//     }
//     QString key2;
//     switch (type) {
//     case CurrentStaticType::MAX: key2 = "MAX"; break;
//     case CurrentStaticType::MIN: key2 = "MIN"; break;
//     default: key = ""; break;
//     }

//     QString cmd = QString("CURRent:STATic:%1? %2").arg(key,key2);
//     if (queryDouble(cmd, value))
//         return value;
//     else
//         return -1;
// }



// void Chroma6310::currStaticRiseFall(double value) {
//     QString cmd = QString("CURR:STAT:RISE %1").arg(value);
//     write(cmd);
// }

// double Chroma6310::queryStaticRiseFall(StaticRiseFallMode mode) {
//     double value = -1;
//     QString key;
//     switch (mode) {
//     case StaticRiseFallMode::RISE: key = "RISE"; break;
//     case StaticRiseFallMode::FALL: key = "FALL"; break;
//     default: key = ""; break;
//     }
//     QString cmd = QString("CURRent:STATic:%1?").arg(key);
//     if (queryDouble(cmd, value))
//         return value;
//     else
//         return -1;
// }

// double Chroma6310::queryStaticRiseFall(StaticRiseFallMode mode,StaticRiseFallType type){
//     double value = -1;
//     QString key;
//     switch (mode) {
//     case StaticRiseFallMode::RISE: key = "RISE"; break;
//     case StaticRiseFallMode::FALL: key = "FALL"; break;
//     default: key = ""; break;
//     }
//     QString key2;
//     switch (type) {
//     case StaticRiseFallType::MAX: key2 = "MAX"; break;
//     case StaticRiseFallType::MIN: key2 = "MIN"; break;
//     default: key = ""; break;
//     }

//     QString cmd = QString("CURRent:STATic:%1? %2").arg(key,key2);
//     if (queryDouble(cmd, value))
//         return value;
//     else
//         return -1;
// }

// void Chroma6310::currDynamicL1L2(CurrentDynamicMode mode, double value) {
//     QString key;
//     switch (mode) {
//     case CurrentDynamicMode::L1: key = "L1"; break;
//     case CurrentDynamicMode::L2: key = "L2"; break;
//     default: key = ""; break;
//     }
//     QString cmd = QString("CURR:DYN:%1 %2").arg(key).arg(value);
//     write(cmd);
// }

// void Chroma6310::currDynamicL1L2(CurrentDynamicMode mode, CurrentDynamicType type) {
//     QString key;
//     switch (mode) {
//     case CurrentDynamicMode::L1: key = "L1"; break;
//     case CurrentDynamicMode::L2: key = "L2"; break;
//     default: key = ""; break;
//     }
//     QString key2;
//     switch (type) {
//     case CurrentDynamicType::MAX: key2 = "MAX"; break;
//     case CurrentDynamicType::MIN: key2 = "MIN"; break;
//     default: key2 = ""; break;
//     }
//     QString cmd = QString("CURR:DYN:%1 %2").arg(key, key2);
//     write(cmd);
// }

// double Chroma6310::querycurrDynamicL1L2(CurrentDynamicMode mode) {
//     double value = -1;
//     QString key;
//     switch (mode) {
//     case CurrentDynamicMode::L1: key = "L1"; break;
//     case CurrentDynamicMode::L2: key = "L2"; break;
//     default: key = ""; break;
//     }
//     QString cmd = QString("CURRent:DYNamic:%1?").arg(key);
//     if (queryDouble(cmd, value))
//         return value;
//     else
//         return -1;
// }

// double Chroma6310::querycurrDynamicL1L2(CurrentDynamicMode mode, CurrentDynamicType type) {
//     double value = -1;
//     QString key;
//     switch (mode) {
//     case CurrentDynamicMode::L1: key = "L1"; break;
//     case CurrentDynamicMode::L2: key = "L2"; break;
//     default: key = ""; break;
//     }
//     QString key2;
//     switch (type) {
//     case CurrentDynamicType::MAX: key2 = "MAX"; break;
//     case CurrentDynamicType::MIN: key2 = "MIN"; break;
//     default: key2 = ""; break;
//     }
//     QString cmd = QString("CURRent:DYNamic:%1? %2").arg(key, key2);
//     if (queryDouble(cmd, value))
//         return value;
//     else
//         return -1;
// }

// void Chroma6310::currDynamicRiseFall(double value) {
//     QString cmd = QString("CURR:DYN:RISE %1").arg(value);
//     write(cmd);
// }

// void Chroma6310::currDynamicRiseFall(DynamicRiseFallMode mode, double value) {
//     QString key;
//     switch (mode) {
//     case DynamicRiseFallMode::RISE: key = "RISE"; break;
//     case DynamicRiseFallMode::FALL: key = "FALL"; break;
//     default: key = ""; break;
//     }
//     QString cmd = QString("CURR:DYN:%1 %2").arg(key).arg(value);
//     write(cmd);
// }

// double Chroma6310::querycurrDynamicRiseFall(DynamicRiseFallMode mode) {
//     double value = -1;
//     QString key;
//     switch (mode) {
//     case DynamicRiseFallMode::RISE: key = "RISE"; break;
//     case DynamicRiseFallMode::FALL: key = "FALL"; break;
//     default: key = ""; break;
//     }
//     QString cmd = QString("CURRent:DYNamic:%1?").arg(key);
//     if (queryDouble(cmd, value))
//         return value;
//     else
//         return -1;
// }

// double Chroma6310::querycurrDynamicRiseFall(DynamicRiseFallMode mode, DynamicRiseFallType type) {
//     double value = -1;
//     QString key;
//     switch (mode) {
//     case DynamicRiseFallMode::RISE: key = "RISE"; break;
//     case DynamicRiseFallMode::FALL: key = "FALL"; break;
//     default: key = ""; break;
//     }
//     QString key2;
//     switch (type) {
//     case DynamicRiseFallType::MAX: key2 = "MAX"; break;
//     case DynamicRiseFallType::MIN: key2 = "MIN"; break;
//     default: key2 = ""; break;
//     }
//     QString cmd = QString("CURRent:DYNamic:%1? %2").arg(key, key2);
//     if (queryDouble(cmd, value))
//         return value;
//     else
//         return -1;
// }

// void Chroma6310::currDynamicT1T2(DynamicTimeMode mode, DynamicTime type)
// {
//     QString key;
//     switch (mode) {
//     case DynamicTimeMode::T1: key = "T1"; break;
//     case DynamicTimeMode::T2: key = "T2"; break;
//     default: key = ""; break;
//     }
//     QString key2;
//     switch (type) {
//     case DynamicTime::S:  key2 = "S"; break;
//     case DynamicTime::mS: key2 = "mS"; break;
//     default: key2 = ""; break;
//     }
//     QString cmd = QString("CURR:DYN:%1 %2").arg(key, key2);
//     write(cmd);
// }

// void Chroma6310::currDynamicT1T2(DynamicTimeMode mode, DynamicTimeType type)
// {
//     QString key;
//     switch (mode) {
//     case DynamicTimeMode::T1: key = "T1"; break;
//     case DynamicTimeMode::T2: key = "T2"; break;
//     default: key = ""; break;
//     }
//     QString key2;
//     switch (type) {
//     case DynamicTimeType::MAX: key2 = "MAX"; break;
//     case DynamicTimeType::MIN: key2 = "MIN"; break;
//     default: key2 = ""; break;
//     }
//     QString cmd = QString("CURR:DYN:%1 %2").arg(key, key2);
//     write(cmd);
// }

// double Chroma6310::querycurrDynamicT1T2(DynamicTimeMode mode)
// {
//     double value = -1;
//     QString key;
//     switch (mode) {
//     case DynamicTimeMode::T1: key = "T1"; break;
//     case DynamicTimeMode::T2: key = "T2"; break;
//     default: key = ""; break;
//     }
//     QString cmd = QString("CURRent:DYNamic:%1?").arg(key);
//     if (queryDouble(cmd, value))
//         return value;
//     else
//         return -1;
// }

// double Chroma6310::querycurrDynamicT1T2(DynamicTimeMode mode, DynamicTimeType type)
// {
//     double value = -1;
//     QString key;
//     switch (mode) {
//     case DynamicTimeMode::T1: key = "T1"; break;
//     case DynamicTimeMode::T2: key = "T2"; break;
//     default: key = ""; break;
//     }
//     QString key2;
//     switch (type) {
//     case DynamicTimeType::MAX: key2 = "MAX"; break;
//     case DynamicTimeType::MIN: key2 = "MIN"; break;
//     default: key2 = ""; break;
//     }
//     QString cmd = QString("CURRent:DYNamic:%1? %2").arg(key, key2);
//     if (queryDouble(cmd, value))
//         return value;
//     else
//         return -1;
// }

// double Chroma6310::fetchVoltage()
// {
//     double value = -1;
//     if (queryDouble("FETCh:VOLTage?", value))
//         return value;
//     else
//         return -1;
// }

// double Chroma6310::fetchCurrent()
// {
//     double value = -1;
//     if (queryDouble("FETCh:CURRent?", value))
//         return value;
//     else
//         return -1;
// }

// int Chroma6310::fetchStatus()
// {
//     int value = -1;
//     if (queryInt("FETCh:STATus?", value))
//         return value;
//     else
//         return -1;
// }

// QVector<double> Chroma6310::fetchAllVoltage()
// {
//     QString result;
//     QVector<double> values;
//     if (queryString("FETCh:ALL:VOLTage?", result)) {
//         // 假設結果是 "12.3,12.4,0,0"
//         QStringList list = result.split(",", Qt::SkipEmptyParts);
//         for (const QString& s : list) {
//             bool ok;
//             double v = s.toDouble(&ok);
//             values.append(ok ? v : -1);
//         }
//     }
//     return values;
// }

// QVector<double> Chroma6310::fetchAllCurrent()
// {
//     QString result;
//     QVector<double> values;
//     if (queryString("FETCh:ALL:CURRent?", result)) {
//         QStringList list = result.split(",", Qt::SkipEmptyParts);
//         for (const QString& s : list) {
//             bool ok;
//             double v = s.toDouble(&ok);
//             values.append(ok ? v : -1);
//         }
//     }
//     return values;
// }

// void Chroma6310::loadState(bool on)
// {
//     QString cmd = QString("LOAD:STATE %1").arg(on ? "ON" : "OFF");
//     write(cmd);
// }


// int Chroma6310::loadState()
// {
//     int value = -1;
//     if (queryInt("LOAD:STATE?", value))
//         return value;
//     else
//         return -1;
// }

// void Chroma6310::shortState(bool on)
// {
//     QString cmd = QString("LOAD:SHORt:STATE %1").arg(on ? "ON" : "OFF");
//     write(cmd);
// }

// int Chroma6310::shortState()
// {
//     int value = -1;
//     if (queryInt("LOAD:SHORt:STATE?", value))
//         return value;
//     else
//         return -1;
// }

// void Chroma6310::shortLoadKey(ShortKeyMode mode)
// {
//     QString key;
//     switch (mode) {
//     case ShortKeyMode::TOGGLE: key = "TOGGLE"; break;
//     case ShortKeyMode::HOLD:   key = "HOLD";   break;
//     default:                   key = "";        break;
//     }
//     QString cmd = QString("LOAD:SHORt:KEY %1").arg(key);
//     write(cmd);
// }

// void Chroma6310::clearLoadProtection()
// {
//     QString cmd = QString("LOAD:PROTection:CLEar");
//     write(cmd);
// }

// void Chroma6310::clearLoad()
// {
//     QString cmd = QString("LOAD:CLEar");
//     write(cmd);
// }

// void Chroma6310::saveLoad()
// {
//     QString cmd = QString("LOAD:SAVe");
//     write(cmd);
// }

// // 量測電壓
// double Chroma6310::measureVoltage()
// {
//     double value = -1;
//     if (queryDouble("MEASure:VOLTage?", value))
//         return value;
//     else
//         return -1;
// }

// // 量測電流
// double Chroma6310::measureCurrent()
// {
//     double value = -1;
//     if (queryDouble("MEASure:CURRent?", value))
//         return value;
//     else
//         return -1;
// }

// // 查詢目前輸入來源 (1=UUT, 0=LOAD, -1=失敗)
// int Chroma6310::measureInput()
// {
//     int value = -1;
//     if (queryInt("MEASure:INPut?", value))
//         return value;
//     else
//         return -1;
// }

// // 設定輸入來源 UUT/LOAD
// void Chroma6310::measureInput(MeasureInputType type)
// {
//     QString key = (type == MeasureInputType::UUT) ? "UUT" : "LOAD";
//     QString cmd = QString("MEASure:INPut %1").arg(key);
//     write(cmd);
// }

// // 查詢 SCAN 狀態 (1=ON, 0=OFF, -1=失敗)
// int Chroma6310::measureScan()
// {
//     int value = -1;
//     if (queryInt("MEASure:SCAN?", value))
//         return value;
//     else
//         return -1;
// }

// // 設定 SCAN ON/OFF
// void Chroma6310::measureScan(bool on)
// {
//     QString cmd = QString("MEASure:SCAN %1").arg(on ? "ON" : "OFF");
//     write(cmd);
// }

// // 量測所有 channel 電壓
// QVector<double> Chroma6310::measureAllVoltage()
// {
//     QString result;
//     QVector<double> values;
//     if (queryString("MEASure:ALLVOLTage?", result)) {
//         QStringList list = result.split(",", Qt::SkipEmptyParts);
//         for (const QString& s : list) {
//             bool ok;
//             double v = s.toDouble(&ok);
//             values.append(ok ? v : -1);
//         }
//     }
//     return values;
// }

// // 量測所有 channel 電流
// QVector<double> Chroma6310::measureAllCurrent()
// {
//     QString result;
//     QVector<double> values;
//     if (queryString("MEASure:ALLCURRent?", result)) {
//         QStringList list = result.split(",", Qt::SkipEmptyParts);
//         for (const QString& s : list) {
//             bool ok;
//             double v = s.toDouble(&ok);
//             values.append(ok ? v : -1);
//         }
//     }
//     return values;
// }

// void Chroma6310::setMode(LoadMode mode)
// {
//     QString key;
//     switch (mode) {
//     case LoadMode::CCL:  key = "CCL";  break;
//     case LoadMode::CCH:  key = "CCH";  break;
//     case LoadMode::CCDL: key = "CCDL"; break;
//     case LoadMode::CCDH: key = "CCDH"; break;
//     case LoadMode::CRL:  key = "CRL";  break;
//     case LoadMode::CRH:  key = "CRH";  break;
//     case LoadMode::CV:   key = "CV";   break;
//     default:             key = "";     break;
//     }
//     QString cmd = QString("MODE %1").arg(key);
//     write(cmd);
// }

// QString Chroma6310::queryloadMode()
// {
//     QString result;
//     if (queryString("MODE?", result))
//         return result;
//     else
//         return QString();
// }

// // FILE
// void Chroma6310::setProgramFile(int index) {
//     QString cmd = QString("PROGram:FILE %1").arg(index);
//     write(cmd);
// }
// int Chroma6310::queryProgramFile() {
//     int value = -1;
//     queryInt("PROGram:FILE?", value);
//     return value;
// }

// // SEQUENCE
// void Chroma6310::setProgramSequence(int index) {
//     QString cmd = QString("PROGram:SEQuence %1").arg(index);
//     write(cmd);
// }
// int Chroma6310::queryProgramSequence() {
//     int value = -1;
//     queryInt("PROGram:SEQuence?", value);
//     return value;
// }

// // SHORT + TIME
// void Chroma6310::setProgramShort(int channel, int value) {
//     QString cmd = QString("PROGram:SHORt:CHANnel %1 %2").arg(channel).arg(value);
//     write(cmd);
// }
// int Chroma6310::queryProgramShort(int channel) {
//     int value = -1;
//     QString cmd = QString("PROGram:SHORt:CHANnel? %1").arg(channel);
//     queryInt(cmd, value);
//     return value;
// }
// void Chroma6310::setProgramShortTime(int channel, double value) {
//     QString cmd = QString("PROGram:SHORt:TIME %1 %2").arg(channel).arg(value);
//     write(cmd);
// }
// double Chroma6310::queryProgramShortTime(int channel) {
//     double value = -1;
//     QString cmd = QString("PROGram:SHORt:TIME? %1").arg(channel);
//     queryDouble(cmd, value);
//     return value;
// }

// // MODE
// void Chroma6310::setProgramMode(ProgramMode mode) {
//     QString key;
//     switch(mode) {
//     case ProgramMode::SKIP: key = "SKIP"; break;
//     case ProgramMode::AUTO: key = "AUTO"; break;
//     case ProgramMode::MANUAL: key = "MANUAL"; break;
//     case ProgramMode::CHAR: key = "CHAR"; break; // 用字串處理
//     default: key = ""; break;
//     }
//     QString cmd = QString("PROGram:MODE %1").arg(key);
//     write(cmd);
// }
// ProgramMode Chroma6310::queryProgramMode() {
//     QString resp;
//     if (queryString("PROGram:MODE?", resp)) {
//         if (resp.contains("SKIP", Qt::CaseInsensitive)) return ProgramMode::SKIP;
//         if (resp.contains("AUTO", Qt::CaseInsensitive)) return ProgramMode::AUTO;
//         if (resp.contains("MANUAL", Qt::CaseInsensitive)) return ProgramMode::MANUAL;
//         return ProgramMode::CHAR;
//     }
//     return ProgramMode::CHAR;
// }

// // ACTive
// void Chroma6310::setProgramActive(int value) {
//     QString cmd = QString("PROGram:ACTive %1").arg(value);
//     write(cmd);
// }
// int Chroma6310::queryProgramActive() {
//     int value = -1;
//     queryInt("PROGram:ACTive?", value);
//     return value;
// }

// // CHAin
// void Chroma6310::setProgramChain(int value) {
//     QString cmd = QString("PROGram:CHAin %1").arg(value);
//     write(cmd);
// }
// int Chroma6310::queryProgramChain() {
//     int value = -1;
//     queryInt("PROGram:CHAin?", value);
//     return value;
// }

// // ----------- PETime -----------
// void Chroma6310::setProgramPETime(int value)
// {
//     QString cmd = QString("PROGram:PETime %1").arg(value);
//     write(cmd);
// }

// double Chroma6310::queryProgramPFDTime()
// {
//     double value = -1;
//     if (queryDouble("PROGram:PETime?", value))
//         return value;
//     else
//         return -1;
// }


// // ----------- ONTime -----------
// void Chroma6310::setProgramONTime(int value)
// {
//     QString cmd = QString("PROGram:ONTime %1").arg(value);
//     write(cmd);
// }

// double Chroma6310::queryProgramONTime()
// {
//     double value = -1;
//     if (queryDouble("PROGram:ONTime?", value))
//         return value;
//     else
//         return -1;
// }


// // ----------- OFFTime -----------
// void Chroma6310::setProgramOFFTime(int value)
// {
//     QString cmd = QString("PROGram:OFFTime %1").arg(value);
//     write(cmd);
// }

// double Chroma6310::queryProgramOFFTime()
// {
//     double value = -1;
//     if (queryDouble("PROGram:OFFTime?", value))
//         return value;
//     else
//         return -1;
// }



// // RUN
// void Chroma6310::programRun() {
//     QString cmd = QString("PROGram:RUN");
//     write(cmd);
// }
// void Chroma6310::programRun(bool on) {
//     QString cmd = QString("PROGram:RUN %1").arg(on ? "ON1" : "OFF0");
//     write(cmd);
// }

// // SAVE
// void Chroma6310::programSave() {
//      QString cmd = QString("PROGram:SAVE");
//     write(cmd);
// }



// // ========== RESISTANCE Subsystem ==========

// // 設定 L1 / L2 阻值
// void Chroma6310::setResistance(ResistanceChannel mode, double value)
// {
//     QString key;
//     switch (mode) {
//     case ResistanceChannel::L1: key = "L1"; break;
//     case ResistanceChannel::L2: key = "L2"; break;
//     default: key = ""; break;
//     }
//     QString cmd = QString("RESistance:%1 %2").arg(key).arg(value);
//     write(cmd);
// }

// // 設定 RISE / FALL 斜率
// void Chroma6310::setResistance(ResistanceSlope mode, double value)
// {
//     QString key;
//     switch (mode) {
//     case ResistanceSlope::RISE: key = "RISE"; break;
//     case ResistanceSlope::FALL: key = "FALL"; break;
//     default: key = ""; break;
//     }
//     QString cmd = QString("RESistance:%1 %2").arg(key).arg(value);
//     write(cmd);
// }

// // 查詢 L1 / L2 阻值
// double Chroma6310::queryResistance(ResistanceChannel mode)
// {
//     double value = -1;
//     QString key;
//     switch (mode) {
//     case ResistanceChannel::L1: key = "L1"; break;
//     case ResistanceChannel::L2: key = "L2"; break;
//     default: key = ""; break;
//     }
//     QString cmd = QString("RESistance:%1?").arg(key);
//     if (queryDouble(cmd, value))
//         return value;
//     else
//         return -1;
// }

// // 查詢 L1 / L2 的 MAX/MIN
// double Chroma6310::queryResistance(ResistanceChannel mode, ResistanceType type)
// {
//     double value = -1;
//     QString key;
//     switch (mode) {
//     case ResistanceChannel::L1: key = "L1"; break;
//     case ResistanceChannel::L2: key = "L2"; break;
//     default: key = ""; break;
//     }
//     QString key2;
//     switch (type) {
//     case ResistanceType::MAX: key2 = "MAX"; break;
//     case ResistanceType::MIN: key2 = "MIN"; break;
//     default: key2 = ""; break;
//     }
//     QString cmd = QString("RESistance:%1? %2").arg(key, key2);
//     if (queryDouble(cmd, value))
//         return value;
//     else
//         return -1;
// }

// // 查詢 RISE / FALL 斜率值
// double Chroma6310::queryResistance(ResistanceSlope mode)
// {
//     double value = -1;
//     QString key;
//     switch (mode) {
//     case ResistanceSlope::RISE: key = "RISE"; break;
//     case ResistanceSlope::FALL: key = "FALL"; break;
//     default: key = ""; break;
//     }
//     QString cmd = QString("RESistance:%1?").arg(key);
//     if (queryDouble(cmd, value))
//         return value;
//     else
//         return -1;
// }

// // 查詢 RISE / FALL 的 MAX/MIN
// double Chroma6310::queryResistance(ResistanceSlope mode, ResistanceType type)
// {
//     double value = -1;
//     QString key;
//     switch (mode) {
//     case ResistanceSlope::RISE: key = "RISE"; break;
//     case ResistanceSlope::FALL: key = "FALL"; break;
//     default: key = ""; break;
//     }
//     QString key2;
//     switch (type) {
//     case ResistanceType::MAX: key2 = "MAX"; break;
//     case ResistanceType::MIN: key2 = "MIN"; break;
//     default: key2 = ""; break;
//     }
//     QString cmd = QString("RESistance:%1? %2").arg(key, key2);
//     if (queryDouble(cmd, value))
//         return value;
//     else
//         return -1;
// }

// void Chroma6310::setRun() {
//     QString cmd = QString("RUN");
//     write(cmd);
// }

// void Chroma6310::showDisplay(ShowDisplayType type)
// {
//     QString key;
//     switch (type) {
//     case ShowDisplayType::L:   key = "L";   break;
//     case ShowDisplayType::R:   key = "R";   break;
//     case ShowDisplayType::LRV: key = "LRV"; break;
//     case ShowDisplayType::LRI: key = "LRI"; break;
//     default:                   key = "";    break;
//     }
//     QString cmd = QString("SHOW:DISPlay %1").arg(key);
//     write(cmd);
// }

// void Chroma6310::setVoltage(VoltageChannel ch, double value)
// {
//     QString key = (ch == VoltageChannel::L1) ? "L1" : "L2";
//     QString cmd = QString("VOLTage:%1 %2").arg(key).arg(value);
//     write(cmd);
// }

// double Chroma6310::queryVoltage(VoltageChannel ch, VoltageQueryType type)
// {
//     double value = -1;
//     QString key = (ch == VoltageChannel::L1) ? "L1" : "L2";
//     QString key2;
//     switch (type) {
//     case VoltageQueryType::MAX: key2 = "MAX"; break;
//     case VoltageQueryType::MIN: key2 = "MIN"; break;
//     default: key2 = ""; break;
//     }
//     QString cmd = key2.isEmpty()
//                       ? QString("VOLTage:%1?").arg(key)
//                       : QString("VOLTage:%1? %2").arg(key, key2);
//     if (queryDouble(cmd, value))
//         return value;
//     else
//         return -1;
// }

// void Chroma6310::setVoltageCurrent(VoltageChannel ch, double value)
// {
//     QString key = (ch == VoltageChannel::L1) ? "L1" : "L2";
//     QString cmd = QString("VOLTage:CURRent:%1 %2").arg(key).arg(value);
//     write(cmd);
// }

// double Chroma6310::queryVoltageCurrent(VoltageChannel ch, VoltageQueryType type)
// {
//     double value = -1;
//     QString key = (ch == VoltageChannel::L1) ? "L1" : "L2";
//     QString key2;
//     switch (type) {
//     case VoltageQueryType::MAX: key2 = "MAX"; break;
//     case VoltageQueryType::MIN: key2 = "MIN"; break;
//     default: key2 = ""; break;
//     }
//     QString cmd = key2.isEmpty()
//                       ? QString("VOLTage:CURRent:%1?").arg(key)
//                       : QString("VOLTage:CURRent:%1? %2").arg(key, key2);
//     if (queryDouble(cmd, value))
//         return value;
//     else
//         return -1;
// }

// void Chroma6310::setVoltageMode(VoltageMode mode)
// {
//     QString key;
//     switch (mode) {
//     case VoltageMode::FAST: key = "FAST"; break;
//     case VoltageMode::SLOW: key = "SLOW"; break;
//     }
//     QString cmd = QString("VOLTage:MODE %1").arg(key);
//     write(cmd);
// }

// VoltageMode Chroma6310::queryVoltageMode()
// {
//     QString resp;
//     if (queryString("VOLTage:MODE?", resp)) {
//         QString trimmed = resp.trimmed().toUpper();
//         if (trimmed == "FAST" || trimmed == "1")
//             return VoltageMode::FAST;
//         else
//             return VoltageMode::SLOW;
//     }
//     // 查詢失敗預設 SLOW
//     return VoltageMode::SLOW;
// }


