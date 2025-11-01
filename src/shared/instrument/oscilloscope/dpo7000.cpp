#include "dpo7000.h"
#include <QDebug>
#include <QThread>


DPO7000::~DPO7000() {
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
    QString cmd = QString("CH%1:STATe %2").arg(channel).arg(enabled ? "ON" : "OFF");
    sendCommandWithLog(cmd, "[DPO7000]");
}

// === 觸發設置方法 ===
void DPO7000::setTriggerType(const QString& type)
{
    m_triggerType = type.toUpper();
    sendCommandWithLog(QString("TRIGger:A:TYPe %1").arg(m_triggerType), "[DPO7000]");
}

void DPO7000::setTriggerSource(const QString& source)
{
    if (m_triggerType == "EDGE") {
        sendCommandWithLog(QString("TRIGger:A:EDGE:SOUrce %1").arg(source.toUpper()), "[DPO7000]");
    } else {
        sendCommandWithLog(QString("TRIGger:A:%1:SOUrce %2").arg(m_triggerType, source.toUpper()), "[DPO7000]");
    }
}

void DPO7000::setTriggerSlope(const QString& slope)
{
    QString normalizedSlope = slope.toUpper();

    if (m_triggerType == "EDGE") {
        if (normalizedSlope == "RISING" || normalizedSlope == "RISing" || normalizedSlope == "POS") {
            sendCommandWithLog("TRIGger:A:EDGE:SLOpe RISe", "[DPO7000]");
        } else if (normalizedSlope == "FALLING" || normalizedSlope == "FALling" || normalizedSlope == "NEG") {
            sendCommandWithLog("TRIGger:A:EDGE:SLOpe FALL", "[DPO7000]");
        } else if (normalizedSlope == "BOTH" || normalizedSlope == "EITher") {
            sendCommandWithLog("TRIGger:A:EDGE:SLOpe EITher", "[DPO7000]");
        } else {
            qWarning() << "[DPO7000] Invalid slope parameter:" << slope
                       << "Valid options: RISING, FALLING, BOTH";
            return;
        }
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
    // 查詢當前模式，根據上次的模式執行
    QString stopAfter = getStopAfterMode();

    qDebug() << "[DPO7000] run() - Current STOPAFTER:" << stopAfter;

    // 如果沒有明確的模式，預設為連續模式
    if (stopAfter.isEmpty() ||
        !stopAfter.contains("SEQUENCE", Qt::CaseInsensitive)) {
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
    sendCommandWithLog("ACQuire:STATE RUN", "[DPO7000]");
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

    // 設定為連續模式
    sendCommandWithLog("ACQuire:STOPAfter RUNSTop", "[DPO7000]");

    // 啟動擷取
    sendCommandWithLog("ACQuire:STATE ON", "[DPO7000]");
}

bool DPO7000::isRunning()
{
    QString response;
    if (!queryString("ACQuire:STATE?", response)) {
        qWarning() << "[DPO7000] isRunning() query failed:" << lastError();
        return false;
    }

    // 移除空白和換行
    response = response.trimmed();

    // qDebug() << "[DPO7000] isRunning() query response:" << response;

    // 判斷是否正在執行
    // 可能的回應: "1", "0", "RUN", "STOP", "ON", "OFF"
    bool running = (response == "1" ||
                    response.contains("RUN", Qt::CaseInsensitive) ||
                    response.contains("ON", Qt::CaseInsensitive));

    // qDebug() << "[DPO7000] isRunning() result:" << running;

    return running;
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
    if (m_triggerType == "EDGE") {
        if (!queryString("TRIGger:A:EDGE:SOUrce?", source)) {
            qWarning() << "[DPO7000] getTriggerSource failed:" << lastError();
            return "";
        }
    } else {
        if (!queryString(QString("TRIGger:A:%1:SOUrce?").arg(m_triggerType), source)) {
            qWarning() << "[DPO7000] getTriggerSource failed for type" << m_triggerType << ":" << lastError();
            return "";
        }
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
    QString slope;
    if (m_triggerType == "EDGE") {
        if (!queryString("TRIGger:A:EDGE:SLOpe?", slope)) {
            qWarning() << "[DPO7000] getTriggerSlope failed:" << lastError();
            return "";
        }
    } else {
        qWarning() << "[DPO7000] Getting slope not supported for trigger type:" << m_triggerType;
        return "";
    }
    return slope.trimmed();
}

double DPO7000::getTriggerLevel()
{
    double level = 0.0;
    if (!queryDouble("TRIGger:A:LEVel?", level)) {
        qWarning() << "[DPO7000] getTriggerLevel failed:" << lastError();
    }
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
    if (!queryDouble(QString("CH%1:POSITION?").arg(channel), pos)) {
        qWarning() << "[DPO7000] getChannelPosition failed:" << lastError();
    }
    return pos;
}

double DPO7000::getHorizontalPosition()
{
    double pos = 0.0;
    if (!queryDouble("HORizontal:POSition?", pos)) {
        qWarning() << "[DPO7000] getHorizontalPosition failed:" << lastError();
    }
    return pos;
}

double DPO7000::getChannelScale(int channel)
{
    double scale = 0.0;
    if (!queryDouble(QString("CH%1:SCALe?").arg(channel), scale)) {
        qWarning() << "[DPO7000] getChannelScale failed for CH" << channel << ":" << lastError();
    }
    return scale;
}

bool DPO7000::isChannelEnabled(int channel)
{
    QString state;
    if (!queryString(QString("CH%1:STATe?").arg(channel), state)) {
        qWarning() << "[DPO7000] isChannelEnabled failed for CH" << channel << ":" << lastError();
        return false;
    }
    return (state.trimmed() == "1" || state.trimmed().toUpper() == "ON");
}

double DPO7000::getTimebase()
{
    double timebase = 0.0;
    if (!queryDouble("HORizontal:SCALe?", timebase)) {
        qWarning() << "[DPO7000] getTimebase failed:" << lastError();
    }
    return timebase;
}

// === 狀態查詢方法（半自動觸發必需） ===
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

// === 系統操作方法 ===
bool DPO7000::waitForOperationComplete(int timeoutMs)
{
    const int POLL_INTERVAL = 50;
    int elapsedTime = 0;

    // 發送操作完成查詢命令
    sendCommandWithLog("*OPC", "[DPO7000]");

    while (elapsedTime < timeoutMs) {
        QString opcResult;
        if (queryString("*OPC?", opcResult)) {
            if (opcResult.trimmed() == "1") {
                return true;
            }
        }

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
    // 設置測量源
    sendCommandWithLog(QString("MEASUrement:IMMed:SOUrce CH%1").arg(channel), "[DPO7000]");

    // 設置測量類型 (MAXimum, MINimum, MEAN, RMS等)
    sendCommandWithLog(QString("MEASUrement:IMMed:TYPe %1").arg(measureType.toUpper()), "[DPO7000]");

    // 獲取測量值
    double value = 0.0;
    if (!queryDouble("MEASUrement:IMMed:VALue?", value)) {
        qWarning() << "[DPO7000] measureSignalPeak failed:" << lastError();
    }

    return value;
}

// === 截圖和波形捕獲方法 ===
QByteArray DPO7000::captureScreenshot(const QString& format)
{
    sendCommandWithLog(QString("HARDCopy:FORMat %1").arg(format.toUpper()), "[DPO7000]");
    sendCommandWithLog("HARDCopy STARt", "[DPO7000]");

    QByteArray img;
    if (!read(img, /*timeoutMs*/ 10000)) {
        qWarning() << "[DPO7000] hardcopy read timeout";
        return {};
    }
    return img;
}

QByteArray DPO7000::captureWaveformFile(int channel,
                                        const QString& format,
                                        const QString& scopePath,
                                        int startPoint,
                                        int stopPoint)
{
    // 1) 選擇來源通道
    sendCommandWithLog(QString("DATa:SOUrce CH%1").arg(channel), "[DPO7000]");

    // (選用) 若要 CSV 帶欄位標頭，打開 HEADER
    if (format.compare("CSV", Qt::CaseInsensitive) == 0) {
        sendCommandWithLog("HEADer ON", "[DPO7000]");
    }

    // 2) 設定保存格式與儀器端檔名
    const QString fmt = format.toUpper();
    sendCommandWithLog(QString("SAVe:WAVEform:FORMat %1").arg(fmt), "[DPO7000]");

    // 3) (選用) 指定要存的點范圍
    if (startPoint > 0) {
        sendCommandWithLog(QString("SAVe:WAVEform:DATa:STARt %1").arg(startPoint), "[DPO7000]");
    }
    if (stopPoint > 0) {
        sendCommandWithLog(QString("SAVe:WAVEform:DATa:STOP %1").arg(stopPoint), "[DPO7000]");
    }

    // 4) 指定儀器端檔案路徑
    const QString quotedPath = QString("\"%1\"").arg(scopePath);
    sendCommandWithLog(QString("SAVe:WAVEform:FILe %1").arg(quotedPath), "[DPO7000]");

    // 5) 執行保存
    sendCommandWithLog("SAVe:WAVEform:DATa", "[DPO7000]");

    // 6) 讀回完整檔案位元流
    QByteArray payload;
    if (!queryBinary(QString("FILESystem:READFile %1").arg(quotedPath), payload)) {
        qWarning() << "[DPO7000] READFILE failed:" << lastError();
        return {};
    }

    // (選用) 順手刪檔
    sendCommandWithLog(QString("FILESystem:DELEte %1").arg(quotedPath), "[DPO7000]");

    return payload;
}

bool DPO7000::captureWaveformFileToHost(int channel,
                                        const QString& hostFilePath,
                                        const QString& format,
                                        const QString& scopePath,
                                        int startPoint,
                                        int stopPoint)
{
    QByteArray fileBytes = captureWaveformFile(channel, format, scopePath, startPoint, stopPoint);
    if (fileBytes.isEmpty()) return false;

    QFile f(hostFilePath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "[DPO7000] open host file failed:" << hostFilePath;
        return false;
    }
    f.write(fileBytes);
    return true;
}
