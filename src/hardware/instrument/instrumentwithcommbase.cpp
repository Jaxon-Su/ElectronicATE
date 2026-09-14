#include "instrumentwithcommbase.h"
#include <QDebug>
#include <QMutexLocker>
#include <QThread>
#include <stdexcept>

void InstrumentWithCommBase::connect()
{
    if (m_comm && (m_comm->isOpen() || m_comm->open())) {
        m_connected = true;
        m_lastError.clear();
    } else {
        m_connected = false;
        m_lastError = m_comm
                          ? QString("Comm open failed: ") + m_comm->lastError()
                                + QString("Comm class failed: InstrumentWithCommBase !")
                          : QString("Comm open failed: no communication object");
        qWarning() << "[Instrument]" << m_lastError;
    }
}

void InstrumentWithCommBase::disconnect()
{
    if (m_comm) {
        m_comm->close();
        if (!m_comm->lastError().isEmpty()) {
            m_lastError = QString("Comm close warning: ") + m_comm->lastError();
            qWarning() << "[Instrument]" << m_lastError;
        } else {
            m_lastError.clear();
        }
    } else {
        m_lastError = "Comm close failed: no communication object";
        qWarning() << "[Instrument]" << m_lastError;
    }
    m_connected = false;
}

bool InstrumentWithCommBase::isConnected() const
{
    return m_connected;
}

void InstrumentWithCommBase::setAddress(const QString& addr)
{
    m_address = addr;
}

QString InstrumentWithCommBase::getaddress() const
{
    return m_address;
}

void InstrumentWithCommBase::setCommunication(ICommunication* comm)
{
    m_comm = comm;
}

int InstrumentWithCommBase::write(const QString& s)
{
    return write(s.toUtf8());
}

int InstrumentWithCommBase::write(const QByteArray& data)
{
    int ret = m_comm ? m_comm->write(data) : -1;
    if (ret < 0 && m_comm)
        m_lastError = QString("Comm write failed: ") + m_comm->lastError();
    return ret;
}

int InstrumentWithCommBase::read(QByteArray& data, int maxLen)
{
    int ret = m_comm ? m_comm->read(data, maxLen) : -1;
    if (ret < 0 && m_comm)
        m_lastError = QString("Comm read failed: ") + m_comm->lastError();
    return ret;
}

bool InstrumentWithCommBase::queryDouble(const QString& cmd, double& value)
{
    QMutexLocker lock(&m_commMutex);
    if (write(cmd) < 0) {
        m_lastError = QString("Write failed: %1").arg(cmd);
        qWarning() << "[Instrument]" << m_lastError;
        return false;
    }
    QByteArray resp;
    int n = read(resp, 64);
    if (n <= 0) {
        m_lastError = QString("Read failed for: %1").arg(cmd);
        qWarning() << "[Instrument]" << m_lastError;
        return false;
    }
    // 取最後一個 token 解析，相容帶 SCPI header 的回應（如 ":CH1:SCALE 1.0E-1"）
    bool ok = false;
    const QStringList tokens = QString(resp).simplified().split(' ', Qt::SkipEmptyParts);
    value = tokens.isEmpty() ? 0.0 : tokens.last().toDouble(&ok);
    if (!ok) {
        m_lastError = QString("Parse failed (not a number): '%1' from %2")
        .arg(QString(resp), cmd);
        qWarning() << "[Instrument]" << m_lastError;
        return false;
    }
    m_lastError.clear();
    return true;
}

bool InstrumentWithCommBase::queryInt(const QString& cmd, int& value)
{
    QMutexLocker lock(&m_commMutex);
    if (write(cmd) < 0) {
        m_lastError = QString("Write failed: %1").arg(cmd);
        qWarning() << "[Instrument]" << m_lastError;
        return false;
    }
    QByteArray resp;
    int n = read(resp, 64);
    if (n <= 0) {
        m_lastError = QString("Read failed for: %1").arg(cmd);
        qWarning() << "[Instrument]" << m_lastError;
        return false;
    }
    // 取最後一個 token 解析：
    //   "25000"                          → "25000"           ✓
    //   ":HORIZONTAL:RECORDLENGTH 25000" → "25000"           ✓ (header ON)
    //   "2.5E+4"                         → "2.5E+4" → 25000  ✓ (科學記號)
    bool ok = false;
    const QStringList tokens = QString(resp).simplified().split(' ', Qt::SkipEmptyParts);
    const double d = tokens.isEmpty() ? 0.0 : tokens.last().toDouble(&ok);
    if (!ok) {
        m_lastError = QString("Parse failed (not a number): '%1' from %2")
        .arg(QString(resp), cmd);
        qWarning() << "[Instrument]" << m_lastError;
        return false;
    }
    value = static_cast<int>(d);
    m_lastError.clear();
    return true;
}

bool InstrumentWithCommBase::queryString(const QString& cmd, QString& result)
{
    QMutexLocker lock(&m_commMutex);
    if (write(cmd) < 0) {
        m_lastError = QString("Write failed: %1").arg(cmd);
        qWarning() << "[Instrument]" << m_lastError;
        return false;
    }
    QByteArray resp;
    int n = read(resp, 256);
    if (n <= 0) {
        m_lastError = QString("Read failed for: %1").arg(cmd);
        qWarning() << "[Instrument]" << m_lastError;
        return false;
    }
    result = QString(resp).trimmed();
    m_lastError.clear();
    return true;
}

bool InstrumentWithCommBase::queryBinary(const QString& cmd, QByteArray& out,
                                         int maxHeaderBytes)
{
    QMutexLocker lock(&m_commMutex);
    out.clear();

    // 1) 送出指令
    if (write(cmd) < 0) {
        m_lastError = QString("Write failed: %1").arg(cmd);
        qWarning() << "[Instrument]" << m_lastError;
        return false;
    }

    // 2) 先讀一小段把 header 讀齊
    QByteArray header;
    QByteArray chunk;
    int n = read(chunk, maxHeaderBytes);
    if (n <= 0) {
        m_lastError = QString("Read failed (header) for: %1").arg(cmd);
        qWarning() << "[Instrument]" << m_lastError;
        return false;
    }
    header.append(chunk);

    // 3) 基本檢查：必須是 binary block
    if (header.isEmpty() || header[0] != '#') {
        m_lastError = QString("Unexpected response (not a binary block) for: %1").arg(cmd);
        qWarning() << "[Instrument]" << m_lastError
                   << " resp head =" << header.left(16);
        return false;
    }

    // 確保至少讀到 2 bytes（'#' + ndig）
    while (header.size() < 2) {
        int m = read(chunk, 1);
        if (m <= 0) {
            m_lastError = "Read failed while completing binary header (ndig)";
            qWarning() << "[Instrument]" << m_lastError;
            return false;
        }
        header.append(chunk.left(m));
    }

    // 4) 解析 <d>（長度位數）
    char c = header[1];
    if (c < '0' || c > '9') {
        m_lastError = "Malformed binary header: ndig not a digit";
        qWarning() << "[Instrument]" << m_lastError;
        return false;
    }
    int ndig = c - '0';
    if (ndig == 0) {
        m_lastError = "Indefinite-length block (#0) not supported";
        qWarning() << "[Instrument]" << m_lastError;
        return false;
    }

    // 5) 確保把 <len> 的字元讀齊
    int headerNeeded = 2 + ndig;
    while (header.size() < headerNeeded) {
        int m = read(chunk, headerNeeded - header.size());
        if (m <= 0) {
            m_lastError = "Read failed while completing length digits";
            qWarning() << "[Instrument]" << m_lastError;
            return false;
        }
        header.append(chunk.left(m));
    }

    // 6) 取出 payload 長度
    bool okLen = false;
    int payloadLen = QString::fromLatin1(header.constData() + 2, ndig).toInt(&okLen);
    if (!okLen || payloadLen < 0) {
        m_lastError = "Malformed binary header: invalid length";
        qWarning() << "[Instrument]" << m_lastError;
        return false;
    }

    qDebug() << "[Instrument] queryBinary: payloadLen =" << payloadLen << "bytes";

    // 7) 把 header 後面在第一包中的「已經攜帶的 payload」先取出
    int alreadyPayload = header.size() - headerNeeded;
    if (alreadyPayload > 0)
        out.append(header.constData() + headerNeeded, alreadyPayload);

    // 8) 繼續把剩下的 payload 讀完
    // 使用 64KB chunk，大幅減少 viRead 呼叫次數，提升 LAN 傳輸效率
    static const int CHUNK_SIZE = 64 * 1024; // 64 KB
    int remain = payloadLen - alreadyPayload;
    out.reserve(payloadLen); // 預先分配記憶體，避免多次 realloc

    while (remain > 0) {
        int toRead = qMin(remain, CHUNK_SIZE);
        int m = read(chunk, toRead);
        if (m <= 0) {
            m_lastError = QString("Read failed while receiving payload, remain=%1").arg(remain);
            qWarning() << "[Instrument]" << m_lastError;
            return false;
        }
        out.append(chunk.constData(), m);
        remain -= m;
    }

    m_lastError.clear();
    qDebug() << "[Instrument] queryBinary: received" << out.size() << "bytes OK";
    return true;
}

void InstrumentWithCommBase::sendCommandWithLog(const QString& cmd, const QString& tag)
{
    QMutexLocker lock(&m_commMutex);
    if (write(cmd) < 0) {
        m_lastError = QString("Write failed: %1").arg(cmd);
        qWarning() << (tag.isEmpty() ? "[Instrument]" : tag) << m_lastError;
    } else {
        m_lastError.clear();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  queryRaw
//  用途：專供回傳「裸 bytes + EOI 結尾」的命令（如 screenshot READFile）
//  注意：不適用於 IEEE 488.2 Binary Block 格式（請改用 queryBinary）
//
//  修正重點：
//   - chunk size 由 4096 → 65536 (64 KB)，大幅減少 viRead 呼叫次數
//   - 最後一次 read() 返回 -1（timeout = 資料已傳完的訊號）屬正常結束，
//     不視為錯誤；以 outData.isEmpty() 判斷是否真的拿到資料
// ─────────────────────────────────────────────────────────────────────────────
bool InstrumentWithCommBase::queryRaw(const QString& cmd, QByteArray& outData)
{
    QMutexLocker lock(&m_commMutex);
    QByteArray cmdBytes = (cmd + "\n").toUtf8();
    if (m_comm->write(cmdBytes) <= 0) {
        m_lastError = "queryRaw write failed";
        qWarning() << "[Instrument]" << m_lastError;
        return false;
    }

    // readFully()：VXI-11 用 VI_SUCCESS END 指示器立即停止，不需等 timeout。
    // 其他通訊層（Serial/GPIB）fallback 到 timeout 驅動的迴圈。
    if (!m_comm->readFully(outData)) {
        m_lastError = QString("queryRaw readFully failed for: %1 (%2)")
                          .arg(cmd, m_comm->lastError());
        qWarning() << "[Instrument]" << m_lastError;
        return false;
    }

    m_lastError.clear();
    return true;
}

void InstrumentWithCommBase::requireOutputOff(const QString& query)
{
    QMutexLocker lock(&m_commMutex);
    for (int attempt = 0; attempt < 3; ++attempt) {
        auto bytes = query.toUtf8();
        if (!bytes.endsWith('\n')) bytes += '\n';
        QByteArray response;
        if (write(bytes) != bytes.size() || read(response, 256) <= 0)
            throw std::runtime_error(QString("%1: output OFF readback failed (%2)").arg(model(), query).toStdString());
        const QString value = QString::fromUtf8(response).trimmed().toUpper();
        bool numeric = false;
        const double state = value.toDouble(&numeric);
        if (value == "OFF" || (numeric && state == 0.0)) return;
        if (value != "ON" && !(numeric && state == 1.0))
            throw std::runtime_error(QString("%1: invalid output state '%2' (%3)").arg(model(), value, query).toStdString());
        if (attempt < 2) QThread::msleep(50);
    }
    throw std::runtime_error(QString("%1: output is still ON after OFF command").arg(model()).toStdString());
}
