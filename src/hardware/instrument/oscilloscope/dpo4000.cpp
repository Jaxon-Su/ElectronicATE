#include "binaryfilestore.h"
#include "dpo4000.h"
#include "tcpcommunication.h"
#include <QDebug>
#include <QThread>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QFileInfo>

// ─────────────────────────────────────────────────────────────────────────────
// MSO4000 / DPO4000 Programmer Manual 對應說明：
//
//   截圖命令（與 DPO7000 EXPort 不同）：
//     SAVe:IMAGe:FILEFormat  {PNG|BMP|JPEG|TIFF}
//     SAVe:IMAGe             "filepath"
//
//   波形儲存（與 DPO7000 相同語法）：
//     SAVe:WAVEform:FILEFormat  {INTERNal|SPREADSHEETcsv|MATHCad}
//     SAVe:WAVEform              <source>, "filepath"
//
//   擷取模式（不含 WFMDB）：
//     ACQuire:MODe  {SAMple|PEAkdetect|HIRes|AVErage|ENVelope}
//
//   觸發位準（通用 + 逐通道）：
//     TRIGger:A:LEVel           <NR3>          ← 套用至目前觸發來源
//     TRIGger:A:LEVel:CH<x>    <NR3>          ← 逐通道個別設定
// ─────────────────────────────────────────────────────────────────────────────

DPO4000::~DPO4000()
{
    disconnect();
}

// ─────────────────────────────────────────────────────────────────────────────
// 基本設置方法
// ─────────────────────────────────────────────────────────────────────────────

void DPO4000::setChannelPosition(int channel, double position)
{
    sendCommandWithLog(QString("CH%1:POSition %2").arg(channel).arg(position), "[DPO4000]");
}

void DPO4000::setHorizontalPosition(double position)
{
    sendCommandWithLog(QString("HORizontal:POSition %1").arg(position), "[DPO4000]");
}

void DPO4000::setChannelCoupling(int channel, const QString& coupling)
{
    sendCommandWithLog(
        QString("CH%1:COUPling %2").arg(channel).arg(coupling.toUpper()),
        "[DPO4000]");
}

void DPO4000::setTimebase(double timePerDiv)
{
    sendCommandWithLog(QString("HORizontal:SCALe %1").arg(timePerDiv), "[DPO4000]");
}

void DPO4000::setChannelScale(int channel, double voltsPerDiv)
{
    sendCommandWithLog(
        QString("CH%1:SCALe %2").arg(channel).arg(voltsPerDiv),
        "[DPO4000]");
}

void DPO4000::enableChannel(int channel, bool enabled)
{
    sendCommandWithLog(
        QString("SELect:CH%1 %2").arg(channel).arg(enabled ? "ON" : "OFF"),
        "[DPO4000]");
}

// ─────────────────────────────────────────────────────────────────────────────
// 觸發設置方法
// ─────────────────────────────────────────────────────────────────────────────

void DPO4000::setTriggerType(const QString& type)
{
    m_triggerType = type.toUpper();
    sendCommandWithLog(QString("TRIGger:A:TYPe %1").arg(m_triggerType), "[DPO4000]");
}

void DPO4000::setTriggerSource(const QString& source)
{
    if (m_triggerType == "EDGE")
        sendCommandWithLog(
            QString("TRIGger:A:EDGE:SOUrce %1").arg(source.toUpper()),
            "[DPO4000]");
    else
        sendCommandWithLog(
            QString("TRIGger:A:%1:SOUrce %2").arg(m_triggerType, source.toUpper()),
            "[DPO4000]");
}

void DPO4000::setTriggerSlope(const QString& slope)
{
    if (m_triggerType != "EDGE") {
        qWarning() << "[DPO4000] setTriggerSlope: slope only valid for EDGE trigger type,"
                   << "current type:" << m_triggerType;
        return;
    }

    const QString upper = slope.toUpper();
    if (upper == "RISING" || upper == "POS")
        sendCommandWithLog("TRIGger:A:EDGE:SLOpe RISe", "[DPO4000]");
    else if (upper == "FALLING" || upper == "NEG")
        sendCommandWithLog("TRIGger:A:EDGE:SLOpe FALL", "[DPO4000]");
    else if (upper == "BOTH" || upper == "EITHER")
        sendCommandWithLog("TRIGger:A:EDGE:SLOpe EITher", "[DPO4000]");
    else
        qWarning() << "[DPO4000] Invalid slope:" << slope
                   << "Valid options: RISING, FALLING, BOTH";
}

void DPO4000::setTriggerLevel(double level)
{
    // 通用版本：套用至目前觸發來源通道
    sendCommandWithLog(QString("TRIGger:A:LEVel %1").arg(level), "[DPO4000]");
}

void DPO4000::setTriggerLevelByChannel(int channel, double level)
{
    // DPO4000 專用：逐通道設定觸發位準
    // SCPI: TRIGger:A:LEVel:CH<x> <NR3>
    sendCommandWithLog(
        QString("TRIGger:A:LEVel:CH%1 %2").arg(channel).arg(level),
        "[DPO4000]");
}

// ─────────────────────────────────────────────────────────────────────────────
// 控制方法
// ─────────────────────────────────────────────────────────────────────────────

void DPO4000::autoSetup()
{
    sendCommandWithLog("AUTOSet EXECute", "[DPO4000]");
}

void DPO4000::automode()
{
    sendCommandWithLog("TRIGger:A:MODe AUTO", "[DPO4000]");
}

void DPO4000::run()
{
    QString stopAfter = getStopAfterMode();
    qDebug() << "[DPO4000] run() - Current STOPAFTER:" << stopAfter;

    if (stopAfter.isEmpty() || !stopAfter.contains("SEQUENCE", Qt::CaseInsensitive)) {
        sendCommandWithLog("ACQuire:STOPAfter RUNSTop", "[DPO4000]");
        qDebug() << "[DPO4000] Set to continuous mode";
    }

    sendCommandWithLog("ACQuire:STATE RUN", "[DPO4000]");
    qDebug() << "[DPO4000] Acquisition started";
}

void DPO4000::stop()
{
    sendCommandWithLog("ACQuire:STATE STOP", "[DPO4000]");
    qDebug() << "[DPO4000] Acquisition stopped";
}

void DPO4000::single()
{
    sendCommandWithLog("ACQuire:STOPAfter SEQuence", "[DPO4000]");
    sendCommandWithLog("ACQuire:STATE RUN",           "[DPO4000]");
}

void DPO4000::normal()
{
    sendCommandWithLog("TRIGger:A:MODe NORMal", "[DPO4000]");
}

void DPO4000::force()
{
    sendCommandWithLog("TRIGger FORCe", "[DPO4000]");
}

void DPO4000::continuous()
{
    qDebug() << "[DPO4000] Starting continuous acquisition";
    sendCommandWithLog("ACQuire:STOPAfter RUNSTop", "[DPO4000]");
    sendCommandWithLog("ACQuire:STATE ON",           "[DPO4000]");
}

// ─────────────────────────────────────────────────────────────────────────────
// 擷取模式
// DPO4000 系列支援：SAMple | PEAkdetect | HIRes | AVErage | ENVelope
// 注意：不支援 WFMDB（DPO7000 專有）
// ─────────────────────────────────────────────────────────────────────────────

void DPO4000::setAcquisitionMode(const QString& mode)
{
    static const QStringList valid = {
        "SAMPLE", "PEAKDETECT", "HIRES", "AVERAGE", "ENVELOPE"
    };

    QString upper = mode.toUpper();
    if (upper == "PEAK") upper = "PEAKDETECT";

    if (!valid.contains(upper)) {
        qWarning() << "[DPO4000] setAcquisitionMode: invalid mode:" << mode
                   << "Valid: SAMple|PEAkdetect|HIRes|AVErage|ENVelope";
        return;
    }
    sendCommandWithLog(QString("ACQuire:MODe %1").arg(upper), "[DPO4000]");
}

QString DPO4000::getAcquisitionMode()
{
    QString mode;
    if (!queryString("ACQuire:MODe?", mode)) {
        qWarning() << "[DPO4000] getAcquisitionMode failed:" << lastError();
        return "";
    }
    return mode.trimmed();
}

bool DPO4000::isRunning()
{
    QString response;
    if (!queryString("ACQuire:STATE?", response)) {
        qWarning() << "[DPO4000] isRunning() query failed:" << lastError();
        return false;
    }
    response = response.trimmed();
    return (response == "1" ||
            response.contains("RUN", Qt::CaseInsensitive) ||
            response.contains("ON",  Qt::CaseInsensitive));
}

QString DPO4000::getStopAfterMode()
{
    QString response;
    if (!queryString("ACQuire:STOPAfter?", response)) {
        qWarning() << "[DPO4000] getStopAfterMode query failed:" << lastError();
        return "";
    }
    QString mode = response.trimmed();
    qDebug() << "[DPO4000] STOPAFTER mode:" << mode;
    return mode;
}

// ─────────────────────────────────────────────────────────────────────────────
// 查詢方法（半自動觸發核心）
// ─────────────────────────────────────────────────────────────────────────────

QString DPO4000::getTriggerSource()
{
    QString source;
    const QString cmd = (m_triggerType == "EDGE")
                            ? "TRIGger:A:EDGE:SOUrce?"
                            : QString("TRIGger:A:%1:SOUrce?").arg(m_triggerType);
    if (!queryString(cmd, source)) {
        qWarning() << "[DPO4000] getTriggerSource failed:" << lastError();
        return "";
    }
    return source.trimmed();
}

QString DPO4000::getTriggerType()
{
    QString type;
    if (!queryString("TRIGger:A:TYPe?", type)) {
        qWarning() << "[DPO4000] getTriggerType failed:" << lastError();
        return "";
    }
    return type.trimmed();
}

QString DPO4000::getTriggerSlope()
{
    if (m_triggerType != "EDGE") {
        qWarning() << "[DPO4000] getTriggerSlope not supported for type:" << m_triggerType;
        return "";
    }
    QString slope;
    if (!queryString("TRIGger:A:EDGE:SLOpe?", slope)) {
        qWarning() << "[DPO4000] getTriggerSlope failed:" << lastError();
        return "";
    }
    return slope.trimmed();
}

double DPO4000::getTriggerLevel()
{
    double level = 0.0;
    if (!queryDouble("TRIGger:A:LEVel?", level))
        qWarning() << "[DPO4000] getTriggerLevel failed:" << lastError();
    return level;
}

QString DPO4000::getTriggerMode()
{
    QString mode;
    if (!queryString("TRIGger:A:MODe?", mode)) {
        qWarning() << "[DPO4000] getTriggerMode failed:" << lastError();
        return "";
    }
    return mode.trimmed();
}

// ─────────────────────────────────────────────────────────────────────────────
// 通道與時基查詢
// ─────────────────────────────────────────────────────────────────────────────

double DPO4000::getChannelPosition(int channel)
{
    double pos = 0.0;
    if (!queryDouble(QString("CH%1:POSition?").arg(channel), pos))
        qWarning() << "[DPO4000] getChannelPosition failed for CH" << channel
                   << ":" << lastError();
    return pos;
}

double DPO4000::getHorizontalPosition()
{
    double pos = 0.0;
    if (!queryDouble("HORizontal:POSition?", pos))
        qWarning() << "[DPO4000] getHorizontalPosition failed:" << lastError();
    return pos;
}

double DPO4000::getChannelScale(int channel)
{
    double scale = 0.0;
    if (!queryDouble(QString("CH%1:SCALe?").arg(channel), scale))
        qWarning() << "[DPO4000] getChannelScale failed for CH" << channel
                   << ":" << lastError();
    return scale;
}

double DPO4000::getTimebase()
{
    double timebase = 0.0;
    if (!queryDouble("HORizontal:SCALe?", timebase))
        qWarning() << "[DPO4000] getTimebase failed:" << lastError();
    return timebase;
}

// ─────────────────────────────────────────────────────────────────────────────
// 私有共用輔助：查詢 SELECT 狀態，「1」/「ON」視為啟用
// ─────────────────────────────────────────────────────────────────────────────

bool DPO4000::querySelectState(const QString& query, const QString& logTag)
{
    QString state;
    if (!queryString(query, state)) {
        qWarning() << logTag << lastError();
        return false;
    }
    qDebug() << "[DPO4000] querySelectState:" << query << "->" << state;
    return (state.trimmed() == "1" || state.trimmed().toUpper() == "ON");
}

bool DPO4000::isChannelEnabled(int channel)
{
    return querySelectState(
        QString("SELect:CH%1?").arg(channel),
        QString("[DPO4000] isChannelEnabled failed for CH%1:").arg(channel));
}

bool DPO4000::isMathChannelEnabled(int channel)
{
    return querySelectState(
        QString("SELect:MATH%1?").arg(channel),
        QString("[DPO4000] isMathChannelEnabled failed for MATH%1:").arg(channel));
}

int DPO4000::getTotalChannel()
{
    return m_totalChannel;
}

// ─────────────────────────────────────────────────────────────────────────────
// 狀態查詢方法
// ─────────────────────────────────────────────────────────────────────────────

QString DPO4000::getAcquisitionState()
{
    QString state;
    if (!queryString("ACQuire:STATE?", state)) {
        qWarning() << "[DPO4000] getAcquisitionState failed:" << lastError();
        return "";
    }
    return state.trimmed();
}

QString DPO4000::getTriggerState()
{
    QString state;
    if (!queryString("TRIGger:STATE?", state)) {
        qWarning() << "[DPO4000] getTriggerState failed:" << lastError();
        return "";
    }
    return state.trimmed();
}

// ─────────────────────────────────────────────────────────────────────────────
// 系統操作方法
// ─────────────────────────────────────────────────────────────────────────────

bool DPO4000::waitForOperationComplete(int timeoutMs)
{
    const int POLL_INTERVAL = 50;
    int elapsedTime = 0;

    sendCommandWithLog("*OPC", "[DPO4000]");

    while (elapsedTime < timeoutMs) {
        QString opcResult;
        if (queryString("*OPC?", opcResult) && opcResult.trimmed() == "1")
            return true;
        QThread::msleep(POLL_INTERVAL);
        elapsedTime += POLL_INTERVAL;
    }

    qWarning() << "[DPO4000] waitForOperationComplete timeout after" << timeoutMs << "ms";
    return false;
}

QString DPO4000::getSystemError()
{
    QString error;
    if (!queryString("SYSTem:ERRor?", error)) {
        qWarning() << "[DPO4000] getSystemError query failed:" << lastError();
        return "";
    }
    return error.trimmed();
}

void DPO4000::clearErrors()
{
    sendCommandWithLog("*CLS", "[DPO4000]");
}

double DPO4000::measureSignalPeak(int channel, const QString& measureType)
{
    sendCommandWithLog(
        QString("MEASUrement:IMMed:SOUrce CH%1").arg(channel),
        "[DPO4000]");
    sendCommandWithLog(
        QString("MEASUrement:IMMed:TYPe %1").arg(measureType.toUpper()),
        "[DPO4000]");
    double value = 0.0;
    if (!queryDouble("MEASUrement:IMMed:VALue?", value))
        qWarning() << "[DPO4000] measureSignalPeak failed:" << lastError();
    return value;
}

// ─────────────────────────────────────────────────────────────────────────────
// captureScreenshot（DPO4000 使用 SAVe:IMAGe，非 DPO7000 的 EXPort）
//
// SCPI 流程：
//   SAVe:IMAGe:FILEFormat  {PNG|BMP|JPEG|TIFF}
//   SAVe:IMAGe             "C:\path\filename.png"
//
// 與 DPO7000 相同：示波器儲存到本機硬碟，PC 端透過 SMB 網路磁碟機讀取
// ─────────────────────────────────────────────────────────────────────────────

QByteArray DPO4000::captureScreenshot(const QString& format, const QString& savePath)
{
    const QString fmt = format.toUpper();
    static const QStringList validFormats = { "BMP", "JPEG", "PNG", "TIFF" };
    const QString actualFmt = validFormats.contains(fmt) ? fmt : "PNG";
    const QString ext       = (actualFmt == "JPEG") ? "jpg" : actualFmt.toLower();

    // 定義帶時間戳的唯一檔名
    const QString fileName = QString("tek_sc_%1.%2")
                                 .arg(QDateTime::currentSecsSinceEpoch())
                                 .arg(ext);

    // 示波器端的 Windows 絕對路徑
    const QString scopeFilePath = QString("C:\\TekScope\\Waveforms\\%1").arg(fileName);
    const QString quotedPath    = QString("\"%1\"").arg(scopeFilePath);

    // PC 端透過 SMB 網路磁碟機讀取的路徑
    QDir mappedDir(m_mappedDrivePath);
    const QString pcFilePath = mappedDir.filePath(fileName);

    // ── 發送截圖命令（DPO4000 使用 SAVe:IMAGe）────────────────────────────
    sendCommandWithLog("*CLS", "[DPO4000]");
    sendCommandWithLog(
        QString("SAVe:IMAGe:FILEFormat %1").arg(actualFmt),
        "[DPO4000]");
    sendCommandWithLog(
        QString("SAVe:IMAGe %1").arg(quotedPath),
        "[DPO4000]");

    // 等待示波器完成寫檔
    if (!waitForOperationComplete(15000)) {
        qWarning() << "[DPO4000] captureScreenshot: OPC 等待逾時";
        return {};
    }

    // ── 透過網路磁碟機（SMB）讀取檔案 ──────────────────────────────────────
    QFile file(pcFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[DPO4000] 無法從網路磁碟機讀取截圖檔:" << pcFilePath
                   << "請確認示波器資料夾是否正確掛載為" << m_mappedDrivePath;
        return {};
    }

    QByteArray img = file.readAll();
    file.close();

    // 讀取後刪除暫存檔以釋放示波器磁碟空間
    QFile::remove(pcFilePath);

    if (img.isEmpty()) {
        qWarning() << "[DPO4000] 讀取到的截圖檔為空";
        return {};
    }

    // 若有指定本地儲存路徑，則另存至 PC
    if (!savePath.isEmpty()) {
        QFile localFile(savePath);
        if (localFile.open(QIODevice::WriteOnly)) {
            localFile.write(img);
            localFile.close();
            qDebug() << "[DPO4000] Screenshot saved to:" << savePath;
        } else {
            qWarning() << "[DPO4000] 無法儲存截圖至指定路徑:" << savePath;
        }
    }

    qDebug() << "[DPO4000] captureScreenshot 成功：" << img.size() << "bytes";
    return img;
}

// ─────────────────────────────────────────────────────────────────────────────
// captureWaveformFile（透過 SMB 網路磁碟機讀取）
//
// DPO4000 格式命令：
//   SAVe:WAVEform:FILEFormat  {INTERNal|SPREADSHEETcsv|MATHCad}
//   SAVe:WAVEform              <source>, "filepath"
// ─────────────────────────────────────────────────────────────────────────────

QByteArray DPO4000::captureWaveformFile(int channel,
                                        const QString& format,
                                        const QString& /*scopePath*/,
                                        int startPoint,
                                        int stopPoint)
{
    // channel == 0 代表擷取所有通道 (ALL)
    const QString chStr = (channel == 0) ? "ALL" : QString("CH%1").arg(channel);
    qDebug() << "[DPO4000] captureWaveformFile:" << chStr << "format=" << format;

    QString fmtCmd;
    QString ext;
    if (format.toUpper() == "CSV") {
        fmtCmd = "SPREADSHEETcsv";
        ext    = "csv";
    } else {
        fmtCmd = "INTERNal";    // 原始 WFM 二進制格式
        ext    = "wfm";
    }

    // 帶時間戳的唯一檔名
    const QString fileName = QString("tek_wave_%1_%2.%3")
                                 .arg(chStr.toLower())
                                 .arg(QDateTime::currentSecsSinceEpoch())
                                 .arg(ext);

    // 示波器端的 Windows 儲存路徑
    const QString scopeFilePath = QString("C:\\TekScope\\Waveforms\\%1").arg(fileName);
    const QString quotedPath    = QString("\"%1\"").arg(scopeFilePath);

    // PC 端的 SMB 讀取路徑
    QDir mappedDir(m_mappedDrivePath);
    const QString pcFilePath = mappedDir.filePath(fileName);

    sendCommandWithLog("*CLS", "[DPO4000]");
    sendCommandWithLog(
        QString("SAVe:WAVEform:FILEFormat %1").arg(fmtCmd),
        "[DPO4000]");

    if (channel != 0)
        sendCommandWithLog(QString("DATa:SOUrce %1").arg(chStr), "[DPO4000]");

    if (startPoint > 0)
        sendCommandWithLog(QString("DATa:STARt %1").arg(startPoint), "[DPO4000]");
    if (stopPoint > 0)
        sendCommandWithLog(QString("DATa:STOP %1").arg(stopPoint),   "[DPO4000]");

    // 執行存檔至示波器
    sendCommandWithLog(
        QString("SAVe:WAVEform %1,%2").arg(chStr).arg(quotedPath),
        "[DPO4000]");

    // CSV 轉換可能耗時，預留 120 秒
    if (!waitForOperationComplete(120000)) {
        qWarning() << "[DPO4000] captureWaveformFile: 存檔逾時（點數過大或轉換耗時過長）";
        return {};
    }

    // ── 透過網路磁碟機直接讀取 ─────────────────────────────────────────────
    QFile file(pcFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[DPO4000] 無法從網路磁碟機讀取波形檔:" << pcFilePath
                   << "請確認資料夾是否正確掛載為" << m_mappedDrivePath;
        return {};
    }

    QByteArray waveData = file.readAll();
    file.close();

    // 讀取完畢後刪除示波器上的暫存檔
    QFile::remove(pcFilePath);

    if (waveData.isEmpty()) {
        qWarning() << "[DPO4000] 讀取到的波形檔為空";
        return {};
    }

    qDebug() << "[DPO4000] captureWaveformFile 成功："
             << waveData.size() / 1024.0 << "KB, CH:" << chStr
             << "format:" << fmtCmd;
    return waveData;
}

// ─────────────────────────────────────────────────────────────────────────────
// captureWaveformFileToHost
// ─────────────────────────────────────────────────────────────────────────────

bool DPO4000::captureWaveformFileToHost(int channel,
                                        const QString& hostFilePath,
                                        const QString& format,
                                        const QString& scopePath,
                                        int startPoint,
                                        int stopPoint)
{
    QByteArray waveData = captureWaveformFile(channel, format, scopePath,
                                              startPoint, stopPoint);
    if (waveData.isEmpty()) {
        qWarning() << "[DPO4000] captureWaveformFileToHost: 無資料可寫入";
        return false;
    }

    const auto saved = BinaryFileStore::save(hostFilePath, waveData);
    if (!saved.succeeded()) {
        qWarning() << "[dpo4000] captureWaveformFileToHost: save failed:"
                   << hostFilePath << saved.detail;
        return false;
    }
    const qint64 written = saved.bytesWritten;
    qDebug() << "[DPO4000] captureWaveformFileToHost 成功:"
             << hostFilePath << written / 1024.0 << "KB";
    return true;
}
