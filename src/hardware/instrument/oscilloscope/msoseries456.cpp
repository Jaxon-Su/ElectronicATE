#include "msoseries456.h"
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QRegularExpression>

// ─────────────────────────────────────────────────────────────────────────────
//  File-local helper: 解析 FILESystem:LDIR? 回傳的檔案大小
//  Tektronix MSO 格式："<filename>",<type>,<size_in_bytes>
//  例："mso_wv_tmp.wfm",4,1048576
//  取最後一個逗號之後的整數即為 byte 數；解析失敗回傳 -1。
// ─────────────────────────────────────────────────────────────────────────────
static qint64 parseLdirSize(const QString& resp)
{
    const int lastComma = resp.lastIndexOf(',');
    if (lastComma < 0) return -1;
    bool ok = false;
    const qint64 sz = resp.mid(lastComma + 1).trimmed().toLongLong(&ok);
    return (ok && sz > 0) ? sz : -1;
}

// ─────────────────────────────────────────────────────────────────────────────
//  File-local helper: 格式化 double 為 Tektronix 慣用科學記號
//  Qt 預設輸出 "1.2500E+09"；Tektronix CSV 使用 "1.2500E+9"（去除指數前導零）
//  未來 captureNativeCsv() 可共用此函數。
// ─────────────────────────────────────────────────────────────────────────────
static QString tekSci(double val, int prec = 4)
{
    if (val == 0.0) return "0";
    QString s = QString::number(val, 'E', prec);
    // 去除指數位的前導零：E+09 → E+9, E-09 → E-9
    static const QRegularExpression re(R"(E([+-])0(\d))");
    s.replace(re, "E\\1\\2");
    return s;
}

// ─────────────────────────────────────────────────────────────────────────────
MSOSeries456::~MSOSeries456()
{
    disconnect();
}

// ═══ 基本設置方法 ════════════════════════════════════════════════════════════

void MSOSeries456::setTimebase(double timePerDiv)
{
    // SCPI: HORizontal:SCAle <NR3>
    // e.g. HORIZONTAL:SCALE 20e-9  → 20 ns/div
    sendCommandWithLog(QString("HORizontal:SCAle %1").arg(timePerDiv), "[MSO456]");
}

void MSOSeries456::setChannelScale(int channel, double voltsPerDiv)
{
    // SCPI: CH<x>:SCAle <NR3>
    sendCommandWithLog(QString("CH%1:SCAle %2").arg(channel).arg(voltsPerDiv), "[MSO456]");
}

void MSOSeries456::setChannelPosition(int channel, double offsetVolts)
{
    // MSO 4/5/6 使用 CH<x>:OFFSet（單位：V），
    // 而非 DPO7000 的 CH<x>:POSITION（單位：div）
    // SCPI: CH<x>:OFFSet <NR3>
    sendCommandWithLog(QString("CH%1:OFFSet %2").arg(channel).arg(offsetVolts), "[MSO456]");
}

void MSOSeries456::setHorizontalPosition(double position)
{
    // SCPI: HORizontal:POSition <NR3>
    sendCommandWithLog(QString("HORizontal:POSition %1").arg(position), "[MSO456]");
}

void MSOSeries456::setChannelCoupling(int channel, const QString& coupling)
{
    // SCPI: CH<x>:COUPling {AC|DC|DCR}
    sendCommandWithLog(
        QString("CH%1:COUPling %2").arg(channel).arg(coupling.toUpper()),
        "[MSO456]");
}

void MSOSeries456::enableChannel(int channel, bool enabled)
{
    // MSO 4/5/6 使用 DISplay:GLObal 命令控制通道顯示，
    // 與 DPO7000 的 SELect:CH<x> 不同
    // SCPI: DISplay:GLObal:CH<x>:STATE {ON|OFF}
    sendCommandWithLog(
        QString("DISplay:GLObal:CH%1:STATE %2").arg(channel).arg(enabled ? "ON" : "OFF"),
        "[MSO456]");
}

// ═══ 觸發設置方法 ════════════════════════════════════════════════════════════

void MSOSeries456::setTriggerType(const QString& type)
{
    // SCPI: TRIGger:{A|B}:TYPe {EDGE|WIDth|TIMEOut|RUNt|WINdow|LOGIc|SETHold|TRANsition|BUS}
    m_triggerType = type.toUpper();
    sendCommandWithLog(QString("TRIGger:A:TYPe %1").arg(m_triggerType), "[MSO456]");
}

void MSOSeries456::setTriggerSource(const QString& source)
{
    // SCPI: TRIGger:{A|B}:EDGE:SOUrce {CH<x>|CH<x>_D<y>|LINE|AUXiliary}
    const QString upper = source.toUpper();
    m_triggerSource = upper;   // 記錄以供 per-channel trigger level 使用

    if (m_triggerType == "EDGE") {
        sendCommandWithLog(
            QString("TRIGger:A:EDGE:SOUrce %1").arg(upper), "[MSO456]");
    } else {
        sendCommandWithLog(
            QString("TRIGger:A:%1:SOUrce %2").arg(m_triggerType, upper),
            "[MSO456]");
    }
}

void MSOSeries456::setTriggerSlope(const QString& slope)
{
    // SCPI: TRIGger:{A|B}:EDGE:SLOpe {RISe|FALL|EITher}
    if (m_triggerType != "EDGE") {
        qWarning() << "[MSO456] setTriggerSlope: slope 僅適用於 EDGE trigger type，"
                      "目前 type =" << m_triggerType;
        return;
    }

    const QString upper = slope.toUpper();
    if (upper == "RISING" || upper == "POS" || upper == "RISE") {
        sendCommandWithLog("TRIGger:A:EDGE:SLOpe RISe", "[MSO456]");
    } else if (upper == "FALLING" || upper == "NEG" || upper == "FALL") {
        sendCommandWithLog("TRIGger:A:EDGE:SLOpe FALL", "[MSO456]");
    } else if (upper == "BOTH" || upper == "EITHER") {
        sendCommandWithLog("TRIGger:A:EDGE:SLOpe EITher", "[MSO456]");
    } else {
        qWarning() << "[MSO456] setTriggerSlope: 無效參數：" << slope
                   << "，有效值：RISING / FALLING / BOTH";
    }
}

void MSOSeries456::setTriggerLevel(double level)
{
    // MSO 4/5/6 的觸發電平是 per-channel 設定
    // SCPI: TRIGger:{A|B}:LEVel:CH<x> <NR3>
    // 與 DPO7000 的全域 TRIGger:A:LEVel <NR3> 不同
    const int ch = parseTriggerChannel(m_triggerSource);
    sendCommandWithLog(
        QString("TRIGger:A:LEVel:CH%1 %2").arg(ch).arg(level),
        "[MSO456]");
}

// ═══ 控制方法 ════════════════════════════════════════════════════════════════

void MSOSeries456::autoSetup()
{
    // SCPI: AUTOSet EXECute
    sendCommandWithLog("AUTOSet EXECute", "[MSO456]");
}

void MSOSeries456::automode()
{
    // SCPI: TRIGger:A:MODe AUTO
    sendCommandWithLog("TRIGger:A:MODe AUTO", "[MSO456]");
}

void MSOSeries456::normal()
{
    // SCPI: TRIGger:A:MODe NORMal
    sendCommandWithLog("TRIGger:A:MODe NORMal", "[MSO456]");
}

void MSOSeries456::run()
{
    // 確保不在 SEQUENCE（single）模式才設 RUNSTOP，避免誤覆蓋單次觸發狀態
    QString stopAfter = getStopAfterMode();
    qDebug() << "[MSO456] run() - 目前 STOPAFTER:" << stopAfter;

    if (stopAfter.isEmpty() || !stopAfter.contains("SEQUENCE", Qt::CaseInsensitive)) {
        sendCommandWithLog("ACQuire:STOPAfter RUNSTop", "[MSO456]");
        qDebug() << "[MSO456] 設定為連續擷取模式";
    }

    // SCPI: ACQuire:STATE RUN
    sendCommandWithLog("ACQuire:STATE RUN", "[MSO456]");
    qDebug() << "[MSO456] 擷取啟動";
}

void MSOSeries456::stop()
{
    // SCPI: ACQuire:STATE STOP
    sendCommandWithLog("ACQuire:STATE STOP", "[MSO456]");
    qDebug() << "[MSO456] 擷取停止";
}

void MSOSeries456::single()
{
    // 等效於面板 SINGLE 按鈕
    // SCPI: ACQuire:STOPAfter SEQuence  →  ACQuire:STATE RUN
    sendCommandWithLog("ACQuire:STOPAfter SEQuence", "[MSO456]");
    sendCommandWithLog("ACQuire:STATE RUN",           "[MSO456]");
}

void MSOSeries456::force()
{
    // SCPI: TRIGger FORCe
    sendCommandWithLog("TRIGger FORCe", "[MSO456]");
}

void MSOSeries456::continuous()
{
    qDebug() << "[MSO456] 啟動連續擷取";
    // SCPI: ACQuire:STOPAfter RUNSTop  →  ACQuire:STATE ON
    sendCommandWithLog("ACQuire:STOPAfter RUNSTop", "[MSO456]");
    sendCommandWithLog("ACQuire:STATE ON",           "[MSO456]");
}

// ═══ 擷取模式 ════════════════════════════════════════════════════════════════

void MSOSeries456::setAcquisitionMode(const QString& mode)
{
    // SCPI: ACQuire:MODe {SAMple|PEAKdetect|HIRes|AVErage|ENVelope}
    // 注意：MSO 4/5/6 不支援 DPO7000 的 WFMDB 模式
    static const QStringList valid = {
        "SAMPLE", "PEAKDETECT", "HIRES", "AVERAGE", "ENVELOPE"
    };

    QString upper = mode.toUpper();
    if (upper == "PEAK") upper = "PEAKDETECT";  // 常見縮寫對應

    if (!valid.contains(upper)) {
        qWarning() << "[MSO456] setAcquisitionMode: 無效模式：" << mode
                   << "，有效值：SAMple|PEAKdetect|HIRes|AVErage|ENVelope";
        return;
    }
    sendCommandWithLog(QString("ACQuire:MODe %1").arg(upper), "[MSO456]");
}

QString MSOSeries456::getAcquisitionMode()
{
    // SCPI: ACQuire:MODe?
    QString mode;
    if (!queryString("ACQuire:MODe?", mode)) {
        qWarning() << "[MSO456] getAcquisitionMode 失敗：" << lastError();
        return "";
    }
    return mode.trimmed();
}

// ═══ 查詢方法（半自動觸發核心） ══════════════════════════════════════════════

bool MSOSeries456::isRunning()
{
    // SCPI: ACQuire:STATE?  → 回傳 0（停止）或 1（執行中）
    QString response;
    if (!queryString("ACQuire:STATE?", response)) {
        qWarning() << "[MSO456] isRunning() 查詢失敗：" << lastError();
        return false;
    }
    response = response.trimmed();
    return (response == "1" ||
            response.contains("RUN", Qt::CaseInsensitive) ||
            response.contains("ON",  Qt::CaseInsensitive));
}

QString MSOSeries456::getStopAfterMode()
{
    // SCPI: ACQuire:STOPAfter?  → RUNSTop 或 SEQuence
    QString response;
    if (!queryString("ACQuire:STOPAfter?", response)) {
        qWarning() << "[MSO456] getStopAfterMode 查詢失敗：" << lastError();
        return "";
    }
    QString mode = response.trimmed();
    qDebug() << "[MSO456] STOPAFTER:" << mode;
    return mode;
}

QString MSOSeries456::getTriggerSource()
{
    // SCPI: TRIGger:A:EDGE:SOUrce?  （EDGE 觸發）
    //       TRIGger:A:<TYPE>:SOUrce?  （其他觸發類型）
    const QString cmd = (m_triggerType == "EDGE")
                            ? "TRIGger:A:EDGE:SOUrce?"
                            : QString("TRIGger:A:%1:SOUrce?").arg(m_triggerType);
    QString source;
    if (!queryString(cmd, source)) {
        qWarning() << "[MSO456] getTriggerSource 失敗：" << lastError();
        return "";
    }
    m_triggerSource = source.trimmed().toUpper();  // 同步快取
    return m_triggerSource;
}

QString MSOSeries456::getTriggerType()
{
    // SCPI: TRIGger:A:TYPe?
    // 回傳 EDGE | WIDth | TIMEOut | RUNt | WINdow | LOGIc | SETHold | TRANsition | BUS
    QString type;
    if (!queryString("TRIGger:A:TYPe?", type)) {
        qWarning() << "[MSO456] getTriggerType 失敗：" << lastError();
        return "";
    }
    m_triggerType = type.trimmed().toUpper();  // 同步快取
    return m_triggerType;
}

QString MSOSeries456::getTriggerSlope()
{
    // SCPI: TRIGger:A:EDGE:SLOpe?  → RISe | FALL | EITher
    if (m_triggerType != "EDGE") {
        qWarning() << "[MSO456] getTriggerSlope: 僅 EDGE 觸發支援 slope 查詢，"
                      "目前 type =" << m_triggerType;
        return "";
    }
    QString slope;
    if (!queryString("TRIGger:A:EDGE:SLOpe?", slope)) {
        qWarning() << "[MSO456] getTriggerSlope 失敗：" << lastError();
        return "";
    }
    return slope.trimmed();
}

double MSOSeries456::getTriggerLevel()
{
    // SCPI: TRIGger:A:LEVel:CH<x>?
    // MSO 4/5/6 的觸發電平是 per-channel，使用目前追蹤的觸發來源通道
    const int ch = parseTriggerChannel(m_triggerSource);
    double level = 0.0;
    if (!queryDouble(QString("TRIGger:A:LEVel:CH%1?").arg(ch), level))
        qWarning() << "[MSO456] getTriggerLevel 失敗（CH" << ch << "）：" << lastError();
    return level;
}

QString MSOSeries456::getTriggerMode()
{
    // SCPI: TRIGger:A:MODe?  → AUTO | NORMal
    QString mode;
    if (!queryString("TRIGger:A:MODe?", mode)) {
        qWarning() << "[MSO456] getTriggerMode 失敗：" << lastError();
        return "";
    }
    return mode.trimmed();
}

// ═══ 通道與時基查詢 ═══════════════════════════════════════════════════════════

double MSOSeries456::getChannelScale(int channel)
{
    // SCPI: CH<x>:SCAle?
    double scale = 0.0;
    if (!queryDouble(QString("CH%1:SCAle?").arg(channel), scale))
        qWarning() << "[MSO456] getChannelScale 失敗（CH" << channel << "）：" << lastError();
    return scale;
}

double MSOSeries456::getChannelPosition(int channel)
{
    // SCPI: CH<x>:OFFSet?  (單位：V，非 div)
    double offset = 0.0;
    if (!queryDouble(QString("CH%1:OFFSet?").arg(channel), offset))
        qWarning() << "[MSO456] getChannelPosition(OFFSet) 失敗（CH" << channel << "）：" << lastError();
    return offset;
}

double MSOSeries456::getHorizontalPosition()
{
    // SCPI: HORizontal:POSition?
    double pos = 0.0;
    if (!queryDouble("HORizontal:POSition?", pos))
        qWarning() << "[MSO456] getHorizontalPosition 失敗：" << lastError();
    return pos;
}

double MSOSeries456::getTimebase()
{
    // SCPI: HORizontal:SCAle?
    double timebase = 0.0;
    if (!queryDouble("HORizontal:SCAle?", timebase))
        qWarning() << "[MSO456] getTimebase 失敗：" << lastError();
    return timebase;
}

// ─────────────────────────────────────────────────────────────────────────────
//  私有輔助：查詢 DISplay:GLObal:<source>:STATE，回傳是否為 ON
// ─────────────────────────────────────────────────────────────────────────────
bool MSOSeries456::queryGlobalDisplayState(const QString& source, const QString& logTag)
{
    // SCPI: DISplay:GLObal:CH<x>:STATE?  → 0 或 1（也可能回傳 OFF / ON）
    const QString query = QString("DISplay:GLObal:%1:STATE?").arg(source);
    QString state;
    if (!queryString(query, state)) {
        qWarning() << logTag << lastError();
        return false;
    }
    qDebug() << "[MSO456] queryGlobalDisplayState:" << query << "->" << state;
    const QString trimmed = state.trimmed();
    return (trimmed == "1" || trimmed.toUpper() == "ON");
}

bool MSOSeries456::isChannelEnabled(int channel)
{
    // SCPI: DISplay:GLObal:CH<x>:STATE?
    return queryGlobalDisplayState(
        QString("CH%1").arg(channel),
        QString("[MSO456] isChannelEnabled 失敗（CH%1）:").arg(channel));
}

bool MSOSeries456::isMathChannelEnabled(int channel)
{
    // SCPI: DISplay:GLObal:MATH<x>:STATE?
    return queryGlobalDisplayState(
        QString("MATH%1").arg(channel),
        QString("[MSO456] isMathChannelEnabled 失敗（MATH%1）:").arg(channel));
}

int MSOSeries456::getTotalChannel()
{
    return m_totalChannel;
}

// ─────────────────────────────────────────────────────────────────────────────
//  私有輔助：從觸發來源字串中萃取通道號
//  "CH2" → 2, "CH1" → 1, "LINE"/"AUX"/unknown → 1（fallback）
// ─────────────────────────────────────────────────────────────────────────────
int MSOSeries456::parseTriggerChannel(const QString& source) const
{
    // 支援格式: "CH1", "CH2", "CH1_D0"（數位通道），"CH3_D5" 等
    QString s = source.trimmed().toUpper();
    if (s.startsWith("CH")) {
        // 取 CH 後的第一個數字群組
        int numStart = 2;
        int numEnd   = numStart;
        while (numEnd < s.size() && s[numEnd].isDigit())
            ++numEnd;
        if (numEnd > numStart) {
            bool ok = false;
            int ch = s.mid(numStart, numEnd - numStart).toInt(&ok);
            if (ok && ch >= 1) return ch;
        }
    }
    qWarning() << "[MSO456] parseTriggerChannel: 無法解析通道號，來源 =" << source << "，fallback 到 CH1";
    return 1;
}

// ═══ 狀態查詢方法 ════════════════════════════════════════════════════════════

QString MSOSeries456::getAcquisitionState()
{
    // SCPI: ACQuire:STATE?  → 0（停止）或 1（執行中）
    QString state;
    if (!queryString("ACQuire:STATE?", state)) {
        qWarning() << "[MSO456] getAcquisitionState 失敗：" << lastError();
        return "";
    }
    return state.trimmed();
}

QString MSOSeries456::getTriggerState()
{
    // SCPI: TRIGger:STATE?
    // 回傳: AUTO | ARMED | READY | TRIGGER | SAVE | SCAN
    QString state;
    if (!queryString("TRIGger:STATE?", state)) {
        qWarning() << "[MSO456] getTriggerState 失敗：" << lastError();
        return "";
    }
    return state.trimmed();
}

// ═══ 系統操作方法 ════════════════════════════════════════════════════════════

bool MSOSeries456::waitForOperationComplete(int timeoutMs)
{
    // *OPC? 在 MSO44B 是阻塞式查詢：儀器直到所有 pending 操作完成才回 "1"。
    // 舊做法（輪詢 + 50ms sleep）無效：每次 *OPC? 都會 block 到 VISA timeout（10s）
    // 才失敗返回，50ms sleep 完全沒有節流效果。
    //
    // 新做法：將 VISA timeout 暫時設為 timeoutMs，發一次 *OPC? 等到底；
    // 操作完成即刻返回，不浪費多餘的 timeout 週期。
    if (m_comm) m_comm->setTimeoutMs(timeoutMs);
    QString result;
    const bool ok = queryString("*OPC?", result) && result.trimmed() == "1";
    //還原timeout setting時間
    if (m_comm) m_comm->setTimeoutMs(10000);

    if (!ok)
        qWarning() << "[MSO456] waitForOperationComplete: 逾時（" << timeoutMs << "ms）";
    return ok;
}

QString MSOSeries456::getSystemError()
{
    // MSO 4/5/6 使用 EVMsg? 取得最新的事件訊息（含錯誤代碼與說明）
    // SCPI: EVMsg?  → "<code>,<message>"
    // 若回傳 "0,No events to report - queue empty" 表示無錯誤
    QString error;
    if (!queryString("EVMsg?", error)) {
        qWarning() << "[MSO456] getSystemError 查詢失敗：" << lastError();
        return "";
    }
    return error.trimmed();
}

void MSOSeries456::clearErrors()
{
    // SCPI: *CLS  清除事件佇列與狀態暫存器
    sendCommandWithLog("*CLS", "[MSO456]");
}

bool MSOSeries456::isClipping(int channel)
{
    // SCPI: CH<x>:CLIPping?  → 0（未超出垂直範圍）或 1（超出）
    QString resp;
    if (!queryString(QString("CH%1:CLIPping?").arg(channel), resp)) {
        qWarning() << "[MSO456] isClipping 查詢失敗（CH" << channel << "）：" << lastError();
        return false;
    }
    return resp.trimmed() == "1";
}

double MSOSeries456::measureSignalPeak(int channel, const QString& measureType)
{
    // MSO 4/5/6 即時量測：先設定量測來源與類型，再讀取結果
    // 若儀器不支援 MEASUrement:IMMed（舊版 legacy 命令），
    // 此呼叫可能回傳錯誤，建議改用 MEASUrement:ADDMEAS 方式。
    // SCPI:
    //   MEASUrement:IMMed:SOUrce1 CH<x>
    //   MEASUrement:IMMed:TYPe <type>
    //   MEASUrement:IMMed:VALue?
    sendCommandWithLog(
        QString("MEASUrement:IMMed:SOUrce1 CH%1").arg(channel), "[MSO456]");
    sendCommandWithLog(
        QString("MEASUrement:IMMed:TYPe %1").arg(measureType.toUpper()), "[MSO456]");

    double value = 0.0;
    if (!queryDouble("MEASUrement:IMMed:VALue?", value))
        qWarning() << "[MSO456] measureSignalPeak 失敗（CH" << channel << "）：" << lastError();
    return value;
}

// ═══ captureScreenshot ══════════════════════════════════════════════════════
//
//  MSO 4/5/6 截圖流程：
//    1. SAVe:IMAGe "<scopeTmpPath>"   → 示波器存到本機暫存路徑
//    2. *OPC? 等待完成
//    3. FILESystem:READFile "<path>"  → 裸 binary 直接回傳到 PC
//    4. FILESystem:DELEte "<path>"    → 刪除暫存檔
//
//  支援格式：PNG / BMP / JPEG（MSO 4/5/6 不支援 PCX、TIFF）
// ─────────────────────────────────────────────────────────────────────────────
QByteArray MSOSeries456::captureScreenshot(const QString& format, const QString& /*savePath*/)
{
    if (m_comm) m_comm->deviceClear();
    sendCommandWithLog("*CLS", "[MSO456]");

    const QString fmt = format.toUpper();

    // 支援的格式；若不符則 fallback 到 PNG
    static const QStringList validFormats = { "PNG", "BMP", "JPEG" };
    const QString actualFmt = validFormats.contains(fmt) ? fmt : "PNG";
    const QString ext       = (actualFmt == "JPEG") ? "jpg" : actualFmt.toLower();

    // 示波器端暫存路徑（固定名稱，用完即刪）
    const QString scopeTmpPath = QString("%1/mso_sc_tmp.%2").arg(m_scopeSaveDir, ext);

    // ── 1. 儲存截圖到示波器本機暫存路徑
    sendCommandWithLog(QString("SAVe:IMAGe \"%1\"").arg(scopeTmpPath), "[MSO456]");

    // ── 2. 等待示波器完成寫檔
    if (!waitForOperationComplete(15000)) {
        qWarning() << "[MSO456] captureScreenshot: OPC 逾時";
        if (m_comm) m_comm->deviceClear();
        return {};
    }

    // ── 3. 透過 FILESystem:READFile 直接讀回 binary（raw bytes，非 IEEE 488.2 block）
    //  MSO 4/5/6 透過 VXI-11 回傳裸 binary（\x89PNG...），不加 # block header，
    //  因此使用 queryRaw 而非 queryBinary。
    QByteArray img;
    if (!queryRaw(QString("FILESystem:READFile \"%1\"").arg(scopeTmpPath), img)) {
        qWarning() << "[MSO456] captureScreenshot: FILESystem:READFile 失敗：" << lastError();
        sendCommandWithLog(QString("FILESystem:DELEte \"%1\"").arg(scopeTmpPath), "[MSO456]");
        if (m_comm) m_comm->deviceClear();
        return {};
    }

    // ── 4. 刪除示波器端暫存檔
    sendCommandWithLog(QString("FILESystem:DELEte \"%1\"").arg(scopeTmpPath), "[MSO456]");

    if (img.isEmpty()) {
        qWarning() << "[MSO456] captureScreenshot: 回傳資料為空";
        if (m_comm) m_comm->deviceClear();
        return {};
    }

    qDebug() << "[MSO456] captureScreenshot 完成：" << img.size() << "bytes";
    return img;
}

// ═══ captureWaveformFile ════════════════════════════════════════════════════
//
//  MSO 4/5/6 波形存檔流程（WFM / MAT 格式）：
//    1. SAVe:WAVEform:GATing NONe          → 確保儲存完整波形
//    2. SAVe:WAVEform CH<x>, "<tmpPath>"   → 示波器存到本機暫存（副檔名決定格式）
//    3. *OPC? 等待完成
//    4. FILESystem:READFile "<tmpPath>"    → 裸 binary 直接回傳到 PC
//    5. FILESystem:DELEte "<tmpPath>"      → 刪除暫存檔
//
//  CSV 單通道格式由 buildCsvViaCurve() 處理（CURVe? binary 路徑）。
//  channel == 0：儲存所有顯示中的通道（SAVe:WAVEform ALL, "<path>"）
//  startPoint / stopPoint 對 SAVe:WAVEform 無效，保留參數供介面相容性。
// ─────────────────────────────────────────────────────────────────────────────
QByteArray MSOSeries456::captureWaveformFile(int channel,
                                             const QString& format,
                                             const QString& /*scopePath*/,
                                             int /*startPoint*/,
                                             int /*stopPoint*/)
{
    // 決定來源字串
    const QString chStr = (channel == 0) ? "ALL"
                                         : QString("CH%1").arg(channel);
    qDebug() << "[MSO456] captureWaveformFile: source=" << chStr << " format=" << format;

    // MSO 4/5/6 以副檔名決定格式
    QString ext;
    const QString fmtUpper = format.toUpper();
    if      (fmtUpper == "CSV") ext = "csv";
    else if (fmtUpper == "WFM") ext = "wfm";
    else if (fmtUpper == "MAT") ext = "mat";
    else {
        qWarning() << "[MSO456] captureWaveformFile: 不支援的格式：" << format
                   << "，改用 CSV";
        ext = "csv";
    }

    // ── CSV 單通道：使用 MSO44B 原生路徑
    //   由示波器執行 SAVe:WAVEform 產生原生 CSV，再透過 FILESystem:READFile
    //   抓回 PC，避免 PC 端以 CURVe? binary 重新後處理 CSV。
    if (ext == "csv" && channel != 0) {
        QByteArray csvData;
        if (!captureNativeCsv(channel, csvData))
            return {};
        return csvData;
    }

    // 示波器端暫存路徑（固定名稱，用完即刪）
    const QString scopeTmpPath = QString("%1/mso_wv_tmp.%2").arg(m_scopeSaveDir, ext);

    // ── 1. 初始化，確保儲存完整波形
    sendCommandWithLog("*CLS", "[MSO456]");
    sendCommandWithLog("SAVe:WAVEform:GATing NONe", "[MSO456]");

    // ── 2. 觸發存檔到示波器本機暫存路徑
    sendCommandWithLog(
        QString("SAVe:WAVEform %1, \"%2\"").arg(chStr, scopeTmpPath),
        "[MSO456]");

    // ── 3. 等待完成（WFM 大量資料點可能耗時較長，預留 120 秒）
    if (!waitForOperationComplete(120000)) {
        qWarning() << "[MSO456] captureWaveformFile: 存檔逾時（資料量過大？）";
        return {};
    }

    // ── 3b. WFM：讀取前先查檔案大小，供讀後完整性驗證
    //   PNG/MAT 資料量小，不需驗證。
    //   WFM 隨 Record Length 增長（可達數 MB），queryRaw 依賴 VXI-11 END 旗標，
    //   MSO44B 韌體在大檔中途可能提前送 END 導致截斷，需以 LDIR 比對確認。
    qint64 expectedWfmSize = -1;
    if (ext == "wfm") {
        QString ldir;
        if (queryString(QString("FILESystem:LDIR? \"%1\"").arg(scopeTmpPath), ldir)) {
            expectedWfmSize = parseLdirSize(ldir);
            qDebug() << "[MSO456] WFM 預期大小：" << expectedWfmSize << "bytes";
        } else {
            qWarning() << "[MSO456] captureWaveformFile: LDIR 查詢失敗，跳過大小驗證";
        }
    }

    // ── 4. 拉高 VISA timeout 再讀大檔
    const int prevTimeoutMs = 10000;
    if (m_comm) m_comm->setTimeoutMs(60000);

    QByteArray waveData;
    if (!queryRaw(QString("FILESystem:READFile \"%1\"").arg(scopeTmpPath), waveData)) {
        if (m_comm) m_comm->setTimeoutMs(prevTimeoutMs);
        qWarning() << "[MSO456] captureWaveformFile: FILESystem:READFile 失敗：" << lastError();
        sendCommandWithLog(QString("FILESystem:DELEte \"%1\"").arg(scopeTmpPath), "[MSO456]");
        return {};
    }

    // ── 4b. 還原 VISA timeout
    if (m_comm) m_comm->setTimeoutMs(prevTimeoutMs);

    // ── 4c. WFM 完整性驗證：比對實際收到的 bytes 與 LDIR 查詢的預期大小
    if (ext == "wfm" && expectedWfmSize > 0 &&
        static_cast<qint64>(waveData.size()) != expectedWfmSize) {
        qWarning() << "[MSO456] captureWaveformFile: WFM 資料截斷！"
                   << "收到" << waveData.size()
                   << "/ 預期" << expectedWfmSize << "bytes";
        sendCommandWithLog(QString("FILESystem:DELEte \"%1\"").arg(scopeTmpPath), "[MSO456]");
        return {};
    }

    // ── 5. 刪除示波器端暫存檔
    sendCommandWithLog(QString("FILESystem:DELEte \"%1\"").arg(scopeTmpPath), "[MSO456]");

    if (waveData.isEmpty()) {
        qWarning() << "[MSO456] captureWaveformFile: 回傳資料為空";
        return {};
    }

    qDebug() << "[MSO456] captureWaveformFile 完成："
             << waveData.size() / 1024.0 << "KB, source=" << chStr << "ext=" << ext;
    return waveData;
}

// ─────────────────────────────────────────────────────────────────────────────
//  captureWaveformFileToHost：儲存波形到本機指定路徑
// ─────────────────────────────────────────────────────────────────────────────
bool MSOSeries456::captureWaveformFileToHost(int channel,
                                             const QString& hostFilePath,
                                             const QString& format,
                                             const QString& scopePath,
                                             int startPoint,
                                             int stopPoint)
{
    QByteArray waveData = captureWaveformFile(channel, format, scopePath,
                                              startPoint, stopPoint);
    if (waveData.isEmpty()) {
        qWarning() << "[MSO456] captureWaveformFileToHost: 無資料可寫入";
        return false;
    }

    QFile file(hostFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "[MSO456] captureWaveformFileToHost: 無法開啟目標檔案：" << hostFilePath;
        return false;
    }

    const qint64 written = file.write(waveData);
    file.close();

    if (written != static_cast<qint64>(waveData.size())) {
        qWarning() << "[MSO456] captureWaveformFileToHost: 寫入不完整："
                   << written << "/" << waveData.size() << "bytes";
        return false;
    }

    qDebug() << "[MSO456] captureWaveformFileToHost 完成："
             << hostFilePath << written / 1024.0 << "KB";
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  captureNativeCsv
//  讓示波器透過 SAVe:WAVEform 寫出原生 CSV，再以 FILESystem:READFile 傳回。
//  關鍵：讀取前執行兩次 deviceClear，清除 VXI-11 output buffer，
//        避免前次未讀完的殘留資料混入本次結果導致截斷或錯誤。
//  若傳輸大檔（> ~400KB）仍失敗，由呼叫端 fallback 到 buildCsvViaCurve()。
// ─────────────────────────────────────────────────────────────────────────────
bool MSOSeries456::captureNativeCsv(int channel, QByteArray& csvOut)
{
    const QString scopeTmpPath =
        QString("%1/mso_nat_ch%2_%3.csv")
            .arg(m_scopeSaveDir)
            .arg(channel)
            .arg(QDateTime::currentDateTimeUtc().toString("yyyyMMdd_HHmmss_zzz"));

    // ── 1. 清除狀態與 output buffer（前次殘留資料的主要防線）
    if (m_comm) m_comm->deviceClear();
    sendCommandWithLog("*CLS", "[MSO456]");
    waitForOperationComplete(5000);

    // ── 2. 讓示波器將原生 CSV 寫到本機暫存路徑（副檔名決定格式）
    sendCommandWithLog("SAVe:WAVEform:GATing NONe", "[MSO456]");
    sendCommandWithLog(
        QString("SAVe:WAVEform CH%1, \"%2\"").arg(channel).arg(scopeTmpPath),
        "[MSO456]");

    // ── 3. 等待示波器完成寫檔（CSV 轉換大量資料點可能耗時，預留 120 秒）
    if (!waitForOperationComplete(120000)) {
        qWarning() << "[MSO456] captureNativeCsv: 存檔逾時";
        sendCommandWithLog(
            QString("FILESystem:DELEte \"%1\"").arg(scopeTmpPath), "[MSO456]");
        return false;
    }

    // ── 4. 拉高 timeout 後透過 VXI-11 讀取原生 CSV
    // MSO44B 實測 FILESystem:READFile CSV 回傳 raw bytes，內容直接從
    // "Model,MSO44B..." 開始，不是 IEEE 488.2 definite-length block。
    if (m_comm) m_comm->setTimeoutMs(60000);
    QByteArray csvData;
    const bool readOk =
        queryRaw(QString("FILESystem:READFile \"%1\"").arg(scopeTmpPath), csvData);
    if (m_comm) m_comm->setTimeoutMs(10000);

    // ── 5. 刪除示波器端暫存檔（無論成功失敗都清理）
    sendCommandWithLog(
        QString("FILESystem:DELEte \"%1\"").arg(scopeTmpPath), "[MSO456]");

    if (!readOk || csvData.isEmpty()) {
        qWarning() << "[MSO456] captureNativeCsv: FILESystem:READFile 失敗，"
                   << "size=" << csvData.size();
        return false;
    }

    csvOut = std::move(csvData);
    qDebug() << "[MSO456] captureNativeCsv 完成："
             << csvOut.size() / 1024.0 << "KB (VXI-11 native)";
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  buildCsvViaCurve
//  直接用 CURVe?（IEEE 488.2 binary block）取波形資料，PC 端計算電壓後組 CSV。
//  原因：FILESystem:READFile 傳輸大型 CSV（> ~400KB）時，MSO44B VXI-11 server
//  提前送 END 導致截斷。CURVe? binary 資料量小（25000 pts = 50KB），
//  單次 64KB VXI-11 chunk 即可完成，徹底避免截斷問題。
//
//  Header 格式盡量對齊 Tektronix 原生 CSV（SAVe:WAVEform 產生），
//  但部分欄位（如 Filter Frequency 設為 FULL 時）無法完全相符。
//  公式：voltage = (raw_adc - YOFf) * YMUlt + YZEro
//        time    = XZEro + i * XINcr
// ─────────────────────────────────────────────────────────────────────────────
bool MSOSeries456::buildCsvViaCurve(int channel, QByteArray& csvOut)
{
    // ── 1. 清空示波器 output buffer
    // viClear（Device Clear）清除儀器 input queue 與 output buffer，
    // 防止前次失敗留下的殘留波形資料在下次 CURVe? 時被誤送出。
    if (m_comm) m_comm->deviceClear();
    sendCommandWithLog("*CLS", "[MSO456]");
    // *OPC? warm-up：確保 VXI-11 session 就緒後再下後續指令
    waitForOperationComplete(5000);

    // ── 2. 設定資料來源
    sendCommandWithLog(QString("DATa:SOUrce CH%1").arg(channel), "[MSO456]");

    // ── 3. 取得目前記錄長度
    int recordLength = 0;
    if (!queryInt("HORizontal:RECOrdlength?", recordLength) || recordLength <= 0) {
        qWarning() << "[MSO456] buildCsvViaCurve: 無法取得 record length";
        if (m_comm) m_comm->deviceClear();
        return false;
    }

    // ── 4. 設定傳輸格式：signed 16-bit big-endian binary（IEEE 488.2 binary block）
    sendCommandWithLog("DATa:ENCdg RIBinary", "[MSO456]");
    sendCommandWithLog("DATa:WIDth 2",        "[MSO456]");
    sendCommandWithLog("DATa:STARt 1",        "[MSO456]");
    sendCommandWithLog(QString("DATa:STOP %1").arg(recordLength), "[MSO456]");

    // ── 5. 查詢 WFMOutpre 波形縮放參數（個別查詢，避免解析複雜合併字串）
    double xIncr = 0, xZero = 0, yMult = 0, yOff = 0, yZero = 0;
    if (!queryDouble("WFMOutpre:XINcr?", xIncr) ||
        !queryDouble("WFMOutpre:XZEro?", xZero) ||
        !queryDouble("WFMOutpre:YMUlt?", yMult) ||
        !queryDouble("WFMOutpre:YOFf?",  yOff)  ||
        !queryDouble("WFMOutpre:YZEro?", yZero)) {
        qWarning() << "[MSO456] buildCsvViaCurve: 無法取得 WFMOutpre 參數";
        if (m_comm) m_comm->deviceClear();
        return false;
    }

    // byte order（RIBinary 預設 MSB，但明確查詢以免韌體差異）
    QString byteOrder;
    bool isMSB = true;
    if (queryString("WFMOutpre:BYT_Or?", byteOrder))
        isMSB = !byteOrder.trimmed().toUpper().startsWith("LSB");

    // ── 6. 查詢 CSV header metadata（對齊 Tektronix 原生格式）

    // --- 6a. *IDN? → 型號 / 韌體版本 ---
    // 格式："TEKTRONIX,MSO44B,C012345,CF:91.1CT FV:1.12.5.156"
    QString modelName = model();   // fallback
    QString firmwareVer;
    {
        QString idn;
        if (queryString("*IDN?", idn)) {
            const QStringList parts = idn.trimmed().split(',');
            if (parts.size() >= 2) modelName   = parts[1].trimmed();
            if (parts.size() >= 4) firmwareVer = parts[3].trimmed();
        }
    }

    // --- 6b. 水平時基 ---
    double hScale = 0.0;
    queryDouble("HORizontal:SCAle?", hScale);

    // --- 6c. 軸單位 ---
    QString xUnit = "s", yUnit = "V";
    {
        QString tmp;
        if (queryString("WFMOutpre:XUNit?", tmp) && !tmp.trimmed().remove('"').isEmpty())
            xUnit = tmp.trimmed().remove('"');
        if (queryString("WFMOutpre:YUNit?", tmp) && !tmp.trimmed().remove('"').isEmpty())
            yUnit = tmp.trimmed().remove('"');
    }

    // --- 6d. 取樣率 ---
    // HORizontal:SAMPLERate? 直接回傳 Sa/s；若查詢失敗則以 1/xIncr 計算。
    double sampleRate = 0.0;
    if (!queryDouble("HORizontal:SAMPLERate?", sampleRate) || sampleRate <= 0.0) {
        if (xIncr > 0.0) sampleRate = 1.0 / xIncr;
    }

    // --- 6e. 頻寬限制 ---
    // CH<x>:BANdwidth? 回傳數值（Hz）或 "FULL"。
    // 原生 CSV 此欄位永遠存在，FULL 頻寬時輸出字串 "FULL"。
    QString filterFreqStr = "FULL";
    {
        QString bw;
        if (queryString(QString("CH%1:BANdwidth?").arg(channel), bw)) {
            const QString bwTrimmed = bw.trimmed();
            bool numOk = false;
            const double v = bwTrimmed.toDouble(&numOk);
            if (numOk && v > 0)
                filterFreqStr = tekSci(v);
        }
    }

    // --- 6f. 探棒衰減 ---
    double probeAtten = 1.0;
    {
        double pa = 0.0;
        if (queryDouble(QString("CH%1:PRObe:ATTenuation?").arg(channel), pa) && pa > 0)
            probeAtten = pa;
    }

    // --- 6g. 垂直 offset / scale（MSO 4/5/6 以 V 為單位）---
    double vOffset = 0.0, vScale = 0.0;
    queryDouble(QString("CH%1:OFFSet?").arg(channel), vOffset);
    queryDouble(QString("CH%1:SCAle?").arg(channel),  vScale);

    // ── 7. 拉高 VISA timeout 後查詢 CURVe?（binary block）
    if (m_comm) m_comm->setTimeoutMs(60000);
    QByteArray rawData;
    const bool curveOk = queryBinary("CURVe?", rawData);
    if (m_comm) m_comm->setTimeoutMs(10000);

    if (!curveOk || rawData.size() < 2) {
        qWarning() << "[MSO456] buildCsvViaCurve: CURVe? 失敗，收到" << rawData.size() << "bytes";
        if (m_comm) m_comm->deviceClear();
        return false;
    }

    const int nPoints = rawData.size() / 2;

    // ── 8. 建構 Tektronix 相容 CSV header
    QByteArray csv;
    csv.reserve(nPoints * 24 + 512);

    csv.append("Model,")             .append(modelName.toUtf8())              .append('\n');
    if (!firmwareVer.isEmpty())
        csv.append("Firmware Version,").append(firmwareVer.toUtf8())          .append('\n');
    csv.append("Point Format,Y\n");
    csv.append("Horizontal Units,")  .append(xUnit.toUtf8())                  .append('\n');
    csv.append("Horizontal Scale,")  .append(tekSci(hScale).toUtf8())         .append('\n');
    csv.append("Sample Interval,")   .append(tekSci(xIncr).toUtf8())          .append('\n');
    csv.append("Sample Rate,")        .append(tekSci(sampleRate).toUtf8())     .append('\n');
    csv.append("Filter Frequency,")  .append(filterFreqStr.toUtf8())          .append('\n');
    csv.append("Record Length,")     .append(QByteArray::number(recordLength)) .append('\n');
    csv.append("Gating,0.0% to 100.0%\n");
    csv.append("Probe Attenuation,") .append(QByteArray::number(probeAtten, 'g')).append('\n');
    csv.append("Vertical Units,")    .append(yUnit.toUtf8())                  .append('\n');
    csv.append("Vertical Offset,")   .append(QByteArray::number(vOffset, 'g')).append('\n');
    csv.append("Vertical Scale,")    .append(tekSci(vScale).toUtf8())         .append('\n');
    csv.append("Vertical Position,") .append(QByteArray::number(vOffset, 'g')).append('\n');
    csv.append("TIME,CH")            .append(QByteArray::number(channel))     .append('\n');

    // ── 9. 解碼 16-bit signed ADC → 時間 / 電壓並附加資料點
    const auto* ptr = reinterpret_cast<const uint8_t*>(rawData.constData());

    for (int i = 0; i < nPoints; ++i) {
        int16_t raw;
        if (isMSB)
            raw = static_cast<int16_t>((ptr[2*i] << 8) | ptr[2*i + 1]);
        else
            raw = static_cast<int16_t>(ptr[2*i] | (ptr[2*i + 1] << 8));

        const double t = xZero + i * xIncr;
        const double v = (static_cast<double>(raw) - yOff) * yMult + yZero;

        csv.append(QString("%1,%2\n").arg(t, 0, 'e', 9).arg(v, 0, 'g', 8).toUtf8());
    }

    csvOut = std::move(csv);
    qDebug() << "[MSO456] buildCsvViaCurve 完成："
             << nPoints << "points," << csvOut.size() / 1024.0 << "KB";
    return true;
}
