#include "dpo7000.h"
#include "tcpcommunication.h"
#include <QDebug>
#include <QThread>
#include <QFile>
#include <QStandardPaths>
#include <QFileInfo>
#include <QDir>

DPO7000::~DPO7000()
{
    disconnect();
}

// === 基本設置方法 ===
void DPO7000::setChannelPosition(int channel, double position)
{
    sendCommandWithLog(QString("CH%1:POSITION %2").arg(channel).arg(position), "[DPO7000]");
}

void DPO7000::setHorizontalPosition(double position)
{
    sendCommandWithLog(QString("HORizontal:POSition %1").arg(position), "[DPO7000]");
}

void DPO7000::setChannelCoupling(int channel, const QString& coupling)
{
    sendCommandWithLog(QString("CH%1:COUPling %2").arg(channel).arg(coupling.toUpper()), "[DPO7000]");
}

void DPO7000::setTimebase(double timePerDiv)
{
    sendCommandWithLog(QString("HORizontal:SCALe %1").arg(timePerDiv), "[DPO7000]");
}

void DPO7000::setChannelScale(int channel, double voltsPerDiv)
{
    sendCommandWithLog(QString("CH%1:SCALe %2").arg(channel).arg(voltsPerDiv), "[DPO7000]");
}

void DPO7000::enableChannel(int channel, bool enabled)
{
    sendCommandWithLog(
        QString("SELect:CH%1 %2").arg(channel).arg(enabled ? "ON" : "OFF"),
        "[DPO7000]");
}

// === 觸發設置方法 ===
void DPO7000::setTriggerType(const QString& type)
{
    m_triggerType = type.toUpper();
    sendCommandWithLog(QString("TRIGger:A:TYPe %1").arg(m_triggerType), "[DPO7000]");
}

void DPO7000::setTriggerSource(const QString& source)
{
    if (m_triggerType == "EDGE")
        sendCommandWithLog(QString("TRIGger:A:EDGE:SOUrce %1").arg(source.toUpper()), "[DPO7000]");
    else
        sendCommandWithLog(
            QString("TRIGger:A:%1:SOUrce %2").arg(m_triggerType, source.toUpper()),
            "[DPO7000]");
}

void DPO7000::setTriggerSlope(const QString& slope)
{
    const QString upper = slope.toUpper();

    if (m_triggerType == "EDGE") {
        if (upper == "RISING" || upper == "POS")
            sendCommandWithLog("TRIGger:A:EDGE:SLOpe RISe", "[DPO7000]");
        else if (upper == "FALLING" || upper == "NEG")
            sendCommandWithLog("TRIGger:A:EDGE:SLOpe FALL", "[DPO7000]");
        else if (upper == "BOTH" || upper == "EITHER")
            sendCommandWithLog("TRIGger:A:EDGE:SLOpe EITher", "[DPO7000]");
        else
            qWarning() << "[DPO7000] Invalid slope parameter:" << slope
                       << "Valid options: RISING, FALLING, BOTH";
    }
}

void DPO7000::setTriggerLevel(double level)
{
    sendCommandWithLog(QString("TRIGger:A:LEVel %1").arg(level), "[DPO7000]");
}

// === 控制方法 ===
void DPO7000::autoSetup()
{
    sendCommandWithLog("AUTOSet EXECute", "[DPO7000]");
}

void DPO7000::automode()
{
    sendCommandWithLog("TRIGger:A:MODe AUTO", "[DPO7000]");
}

void DPO7000::run()
{
    QString stopAfter = getStopAfterMode();
    qDebug() << "[DPO7000] run() - Current STOPAFTER:" << stopAfter;

    if (stopAfter.isEmpty() || !stopAfter.contains("SEQUENCE", Qt::CaseInsensitive)) {
        sendCommandWithLog("ACQuire:STOPAfter RUNSTop", "[DPO7000]");
        qDebug() << "[DPO7000] Set to continuous mode";
    }

    sendCommandWithLog("ACQuire:STATE RUN", "[DPO7000]");
    qDebug() << "[DPO7000] Acquisition started";
}

void DPO7000::stop()
{
    sendCommandWithLog("ACQuire:STATE STOP", "[DPO7000]");
    qDebug() << "[DPO7000] Acquisition stopped";
}

void DPO7000::single()
{
    sendCommandWithLog("ACQuire:STOPAfter SEQuence", "[DPO7000]");
    sendCommandWithLog("ACQuire:STATE RUN",           "[DPO7000]");
}

void DPO7000::normal()
{
    sendCommandWithLog("TRIGger:A:MODe NORMal", "[DPO7000]");
}

void DPO7000::force()
{
    sendCommandWithLog("TRIGger FORCe", "[DPO7000]");
}

void DPO7000::continuous()
{
    qDebug() << "[DPO7000] Starting continuous acquisition";
    sendCommandWithLog("ACQuire:STOPAfter RUNSTop", "[DPO7000]");
    sendCommandWithLog("ACQuire:STATE ON",           "[DPO7000]");
}

// === 擷取模式 ===
void DPO7000::setAcquisitionMode(const QString& mode)
{
    static const QStringList valid = {
        "SAMPLE", "PEAKDETECT", "HIRES", "AVERAGE", "ENVELOPE", "WFMDB"
    };

    QString upper = mode.toUpper();
    if (upper == "PEAK") upper = "PEAKDETECT";

    if (!valid.contains(upper)) {
        qWarning() << "[DPO7000] setAcquisitionMode: invalid mode:" << mode
                   << "Valid: SAMple|PEAKdetect|HIRes|AVErage|ENVelope|WFMDB";
        return;
    }
    sendCommandWithLog(QString("ACQuire:MODe %1").arg(upper), "[DPO7000]");
}

QString DPO7000::getAcquisitionMode()
{
    QString mode;
    if (!queryString("ACQuire:MODe?", mode)) {
        qWarning() << "[DPO7000] getAcquisitionMode failed:" << lastError();
        return "";
    }
    return mode.trimmed();
}

bool DPO7000::isRunning()
{
    QString response;
    if (!queryString("ACQuire:STATE?", response)) {
        qWarning() << "[DPO7000] isRunning() query failed:" << lastError();
        return false;
    }
    response = response.trimmed();
    return (response == "1" ||
            response.contains("RUN", Qt::CaseInsensitive) ||
            response.contains("ON",  Qt::CaseInsensitive));
}

QString DPO7000::getStopAfterMode()
{
    QString response;
    if (!queryString("ACQuire:STOPAfter?", response)) {
        qWarning() << "[DPO7000] getStopAfterMode query failed:" << lastError();
        return "";
    }
    QString mode = response.trimmed();
    qDebug() << "[DPO7000] STOPAFTER mode:" << mode;
    return mode;
}

// === 查詢方法（半自動觸發核心） ===
QString DPO7000::getTriggerSource()
{
    QString source;
    const QString cmd = (m_triggerType == "EDGE")
                            ? "TRIGger:A:EDGE:SOUrce?"
                            : QString("TRIGger:A:%1:SOUrce?").arg(m_triggerType);
    if (!queryString(cmd, source)) {
        qWarning() << "[DPO7000] getTriggerSource failed:" << lastError();
        return "";
    }
    return source.trimmed();
}

QString DPO7000::getTriggerType()
{
    QString type;
    if (!queryString("TRIGger:A:TYPe?", type)) {
        qWarning() << "[DPO7000] getTriggerType failed:" << lastError();
        return "";
    }
    return type.trimmed();
}

QString DPO7000::getTriggerSlope()
{
    if (m_triggerType != "EDGE") {
        qWarning() << "[DPO7000] Getting slope not supported for trigger type:" << m_triggerType;
        return "";
    }
    QString slope;
    if (!queryString("TRIGger:A:EDGE:SLOpe?", slope)) {
        qWarning() << "[DPO7000] getTriggerSlope failed:" << lastError();
        return "";
    }
    return slope.trimmed();
}

double DPO7000::getTriggerLevel()
{
    double level = 0.0;
    if (!queryDouble("TRIGger:A:LEVel?", level))
        qWarning() << "[DPO7000] getTriggerLevel failed:" << lastError();
    return level;
}

QString DPO7000::getTriggerMode()
{
    QString mode;
    if (!queryString("TRIGger:A:MODe?", mode)) {
        qWarning() << "[DPO7000] getTriggerMode failed:" << lastError();
        return "";
    }
    return mode.trimmed();
}

// === 通道和時基查詢 ===
double DPO7000::getChannelPosition(int channel)
{
    double pos = 0.0;
    if (!queryDouble(QString("CH%1:POSITION?").arg(channel), pos))
        qWarning() << "[DPO7000] getChannelPosition failed:" << lastError();
    return pos;
}

double DPO7000::getHorizontalPosition()
{
    double pos = 0.0;
    if (!queryDouble("HORizontal:POSition?", pos))
        qWarning() << "[DPO7000] getHorizontalPosition failed:" << lastError();
    return pos;
}

double DPO7000::getChannelScale(int channel)
{
    double scale = 0.0;
    if (!queryDouble(QString("CH%1:SCALe?").arg(channel), scale))
        qWarning() << "[DPO7000] getChannelScale failed for CH" << channel << ":" << lastError();
    return scale;
}

// ─────────────────────────────────────────────────────────────────────────────
// 私有共用輔助：查詢 SELECT 狀態暫存器，啟用則回傳 true
// 示波器回傳值為「1」/「0」或「ON」/「OFF」
// ─────────────────────────────────────────────────────────────────────────────
bool DPO7000::querySelectState(const QString& query, const QString& logTag)
{
    QString state;
    if (!queryString(query, state)) {
        qWarning() << logTag << lastError();
        return false;
    }
    qDebug() << "[DPO7000] querySelectState:" << query << "->" << state;
    return (state.trimmed() == "1" || state.trimmed().toUpper() == "ON");
}

bool DPO7000::isChannelEnabled(int channel)
{
    return querySelectState(
        QString("SELect:CH%1?").arg(channel),
        QString("[DPO7000] isChannelEnabled failed for CH%1:").arg(channel)
        );
}

bool DPO7000::isMathChannelEnabled(int channel)
{
    return querySelectState(
        QString("SELect:MATH%1?").arg(channel),
        QString("[DPO7000] isMathChannelEnabled failed for MATH%1:").arg(channel)
        );
}

double DPO7000::getTimebase()
{
    double timebase = 0.0;
    if (!queryDouble("HORizontal:SCALe?", timebase))
        qWarning() << "[DPO7000] getTimebase failed:" << lastError();
    return timebase;
}

// === 狀態查詢方法 ===
QString DPO7000::getAcquisitionState()
{
    QString state;
    if (!queryString("ACQuire:STATE?", state)) {
        qWarning() << "[DPO7000] getAcquisitionState failed:" << lastError();
        return "";
    }
    return state.trimmed();
}

QString DPO7000::getTriggerState()
{
    QString state;
    if (!queryString("TRIGger:STATE?", state)) {
        qWarning() << "[DPO7000] getTriggerState failed:" << lastError();
        return "";
    }
    return state.trimmed();
}

int DPO7000::getTotalChannel()
{
    return m_totalChannel;
}

// === 系統操作方法 ===
bool DPO7000::waitForOperationComplete(int timeoutMs)
{
    const int POLL_INTERVAL = 50;
    int elapsedTime = 0;

    sendCommandWithLog("*OPC", "[DPO7000]");

    while (elapsedTime < timeoutMs) {
        QString opcResult;
        if (queryString("*OPC?", opcResult) && opcResult.trimmed() == "1")
            return true;
        QThread::msleep(POLL_INTERVAL);
        elapsedTime += POLL_INTERVAL;
    }

    qWarning() << "[DPO7000] waitForOperationComplete timeout after" << timeoutMs << "ms";
    return false;
}

QString DPO7000::getSystemError()
{
    QString error;
    if (!queryString("SYSTem:ERRor?", error)) {
        qWarning() << "[DPO7000] getSystemError query failed:" << lastError();
        return "";
    }
    return error.trimmed();
}

void DPO7000::clearErrors()
{
    sendCommandWithLog("*CLS", "[DPO7000]");
}

double DPO7000::measureSignalPeak(int channel, const QString& measureType)
{
    sendCommandWithLog(QString("MEASUrement:IMMed:SOUrce CH%1").arg(channel), "[DPO7000]");
    sendCommandWithLog(QString("MEASUrement:IMMed:TYPe %1").arg(measureType.toUpper()), "[DPO7000]");
    double value = 0.0;
    if (!queryDouble("MEASUrement:IMMed:VALue?", value))
        qWarning() << "[DPO7000] measureSignalPeak failed:" << lastError();
    return value;
}

// ─────────────────────────────────────────────────────────────────────────────
//  captureScreenshot (利用 SMB 網路磁碟機讀取)
// ─────────────────────────────────────────────────────────────────────────────
//徹底放棄使用 VISA (FILESystem:READFile) 來傳輸大檔案。
//既然已經在 PC 端將示波器的 C:\TekScope\Waveforms 資料夾掛載為網路磁碟機 Z:，直接讓示波器把檔案存到該目錄，然後 PC 端用 Qt 的 QFile 直接從 Z: 槽讀取。這會將原本需要數十秒的傳輸時間縮短到毫秒級。
QByteArray DPO7000::captureScreenshot(const QString& format, const QString& savePath)
{
    const QString fmt = format.toUpper();
    static const QStringList validFormats = { "BMP", "JPEG", "PCX", "PNG", "TIFF" };
    const QString actualFmt = validFormats.contains(fmt) ? fmt : "PNG";
    const QString ext       = (actualFmt == "JPEG") ? "jpg" : actualFmt.toLower();

    // 定義檔名
    const QString fileName = QString("tek_sc_%1.%2").arg(QDateTime::currentSecsSinceEpoch()).arg(ext);

    // 示波器端的絕對路徑 (假設 C:\TekScope\Waveforms 是被分享的根目錄)
    const QString scopeFilePath = QString("C:\\TekScope\\Waveforms\\%1").arg(fileName);
    const QString quotedPath = QString("\"%1\"").arg(scopeFilePath);

    // PC 端的讀取路徑 (利用掛載的 Z: 槽)
    QDir mappedDir(m_mappedDrivePath);
    const QString pcFilePath = mappedDir.filePath(fileName);

    sendCommandWithLog("*CLS",                                                "[DPO7000]");
    sendCommandWithLog(QString("EXPort:FORMat %1").arg(actualFmt),            "[DPO7000]");
    sendCommandWithLog("EXPort:VIEW FULLSCREEN",                              "[DPO7000]");
    sendCommandWithLog("EXPort:PALEtte COLOr",                                "[DPO7000]");
    sendCommandWithLog(QString("EXPort:FILEName %1").arg(quotedPath),         "[DPO7000]");
    sendCommandWithLog("EXPort STARt",                                        "[DPO7000]");

    // 等待示波器繪圖並將檔案寫入硬碟
    if (!waitForOperationComplete(15000)) {
        qWarning() << "[DPO7000] captureScreenshot: OPC 等待逾時";
        return {};
    }

    // --- 透過網路磁碟機 (SMB) 讀取檔案，速度極快 ---
    QFile file(pcFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[DPO7000] 無法從網路磁碟機讀取截圖檔:" << pcFilePath
                   << "請確認示波器資料夾是否正確掛載為" << m_mappedDrivePath;
        return {};
    }

    QByteArray img = file.readAll();
    file.close();

    // 讀取成功後，刪除示波器端/PC端的原始檔案以釋放空間
    QFile::remove(pcFilePath);

    if (img.isEmpty()) {
        qWarning() << "[DPO7000] 讀取到的截圖檔為空";
        return {};
    }

    // 若有指定儲存路徑，則存到指定的 Local PC 目錄
    if (!savePath.isEmpty()) {
        QFile localFile(savePath);
        if (localFile.open(QIODevice::WriteOnly)) {
            localFile.write(img);
            localFile.close();
            qDebug() << "[DPO7000] Screenshot saved to:" << savePath;
        }
    }

    qDebug() << "[DPO7000] captureScreenshot 成功：" << img.size() << "bytes";
    return img;
}

// ─────────────────────────────────────────────────────────────────────────────
//  captureWaveformFile (優化版：利用 SMB 網路磁碟機讀取)
// ─────────────────────────────────────────────────────────────────────────────
QByteArray DPO7000::captureWaveformFile(int channel,
                                        const QString& format,
                                        const QString& /*scopePath*/,
                                        int startPoint,
                                        int stopPoint)
{
    // channel == 0 代表擷取所有通道 (ALL)
    QString chStr = (channel == 0) ? "ALL" : QString("CH%1").arg(channel);
    qDebug() << "[DPO7000] captureWaveformFile:" << chStr << "format=" << format;

    // 關鍵修正：DPO7000 必須明確指定 SPREADSHEETCSV，否則會預設存成二進制 WFM
    QString fmtCmd;
    QString ext;
    if (format.toUpper() == "CSV") {
        fmtCmd = "SPREADSHEETCSV"; // 修正此處
        ext = "csv";
    } else {
        fmtCmd = "INTERNal"; // 若非 CSV，則使用原始內部二進制格式
        ext = "wfm";
    }

    // 定義檔名 (加入時間戳防撞名)
    const QString fileName = QString("tek_wave_%1_%2.%3").arg(chStr.toLower()).arg(QDateTime::currentSecsSinceEpoch()).arg(ext);

    // 示波器端的儲存路徑
    const QString scopeFilePath = QString("C:\\TekScope\\Waveforms\\%1").arg(fileName);
    const QString quotedPath    = QString("\"%1\"").arg(scopeFilePath);

    // PC 端的讀取路徑
    QDir mappedDir(m_mappedDrivePath);
    const QString pcFilePath = mappedDir.filePath(fileName);

    sendCommandWithLog("*CLS", "[DPO7000]");

    // 設定正確的檔案格式
    sendCommandWithLog(QString("SAVe:WAVEform:FILEFormat %1").arg(fmtCmd), "[DPO7000]");

    if (channel != 0) {
        sendCommandWithLog(QString("DATa:SOUrce %1").arg(chStr), "[DPO7000]");
    }

    if (startPoint > 0)
        sendCommandWithLog(QString("DATa:STARt %1").arg(startPoint), "[DPO7000]");
    if (stopPoint > 0)
        sendCommandWithLog(QString("DATa:STOP %1").arg(stopPoint),   "[DPO7000]");

    // 執行存檔至示波器內部硬碟
    sendCommandWithLog(QString("SAVe:WAVEform %1, %2").arg(chStr).arg(quotedPath), "[DPO7000]");

    // 讓示波器處理 50 萬點轉 CSV 是純粹的 CPU 苦力活，可能會耗時幾十秒
    // 這裡預留了 120 秒的等待時間，確保它轉檔完成
    if (!waitForOperationComplete(120000)) {
        qWarning() << "[DPO7000] captureWaveformFile: 存檔逾時（點數過大，轉換耗時過長）";
        return {};
    }

    // --- 透過網路磁碟機直接讀取 ---
    QFile file(pcFilePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[DPO7000] 無法從網路磁碟機讀取波形檔:" << pcFilePath
                   << "請確認資料夾是否正確掛載為" << m_mappedDrivePath;
        return {};
    }

    QByteArray waveData = file.readAll();
    file.close();

    // 讀取完畢後，刪除示波器硬碟上的暫存檔
    QFile::remove(pcFilePath);

    if (waveData.isEmpty()) {
        qWarning() << "[DPO7000] 讀取到的波形檔為空";
        return {};
    }

    qDebug() << "[DPO7000] captureWaveformFile 成功："
             << waveData.size() / 1024.0 << "KB, CH:" << chStr << fmtCmd;

    return waveData;
}
// ─────────────────────────────────────────────────────────────────────────────
//  captureWaveformFileToHost
// ─────────────────────────────────────────────────────────────────────────────
bool DPO7000::captureWaveformFileToHost(int channel,
                                        const QString& hostFilePath,
                                        const QString& format,
                                        const QString& scopePath,
                                        int startPoint,
                                        int stopPoint)
{
    QByteArray waveData = captureWaveformFile(channel, format, scopePath,
                                              startPoint, stopPoint);
    if (waveData.isEmpty()) {
        qWarning() << "[DPO7000] captureWaveformFileToHost: 無資料可寫入";
        return false;
    }

    QFile file(hostFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "[DPO7000] captureWaveformFileToHost: 無法開啟檔案:" << hostFilePath;
        return false;
    }

    qint64 written = file.write(waveData);
    file.close();

    if (written != static_cast<qint64>(waveData.size())) {
        qWarning() << "[DPO7000] captureWaveformFileToHost: 寫入不完整"
                   << written << "/" << waveData.size() << "bytes";
        return false;
    }

    qDebug() << "[DPO7000] captureWaveformFileToHost 成功:"
             << hostFilePath << written / 1024.0 << "KB";
    return true;
}
