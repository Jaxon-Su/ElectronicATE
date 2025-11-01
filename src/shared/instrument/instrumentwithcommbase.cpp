#include "instrumentwithcommbase.h"
#include <QDebug>

void InstrumentWithCommBase::connect()
{
    if (m_comm && m_comm->open()) {
        m_connected = true;
        m_lastError.clear();
    } else {
        m_connected = false;
        m_lastError = m_comm ? QString("Comm open failed: ") + m_comm->lastError() + QString("Comm class failed: InstrumentWithCommBase !")
                             : QString("Comm open failed: no communication object");
        qWarning() << "[Instrument]" << m_lastError;

    }
}

void InstrumentWithCommBase::disconnect()
{
    if (m_comm) {
        m_comm->close();
        // 假如 close 會有失敗（可由 m_comm->lastError() 判斷），可記錄
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

void InstrumentWithCommBase::setCommunication(ICommunication* comm){
    m_comm = comm;
}

int InstrumentWithCommBase::write(const QString& s) {
    return write(s.toUtf8());
}

int InstrumentWithCommBase::write(const QByteArray& data) {
    int ret = m_comm ? m_comm->write(data) : -1;
    if (ret < 0 && m_comm) {
        m_lastError = QString("Comm write failed: ") + m_comm->lastError();
    }
    return ret;
}

int InstrumentWithCommBase::read(QByteArray& data, int maxLen) {
    int ret = m_comm ? m_comm->read(data, maxLen) : -1;
    if (ret < 0 && m_comm) {
        m_lastError = QString("Comm read failed: ") + m_comm->lastError();
    }
    return ret;
}

bool InstrumentWithCommBase::queryDouble(const QString& cmd, double& value) {
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
    bool ok = false;
    value = QString(resp).toDouble(&ok);
    if (!ok) {
        m_lastError = QString("Parse failed (not a number): '%1' from %2").arg(QString(resp)).arg(cmd);
        qWarning() << "[Instrument]" << m_lastError;
        return false;
    }
    m_lastError.clear();
    return true;
}


bool InstrumentWithCommBase::queryInt(const QString& cmd, int& value) {
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
    bool ok = false;
    value = QString(resp).simplified().toInt(&ok); // simplified() 避免亂碼空白
    if (!ok) {
        m_lastError = QString("Parse failed (not an int): '%1' from %2").arg(QString(resp)).arg(cmd);
        qWarning() << "[Instrument]" << m_lastError;
        return false;
    }
    m_lastError.clear();
    return true;
}

bool InstrumentWithCommBase::queryString(const QString& cmd, QString& result) {
    if (write(cmd) < 0) {
        m_lastError = QString("Write failed: %1").arg(cmd);
        qWarning() << "[Instrument]" << m_lastError;
        return false;
    }
    QByteArray resp;
    int n = read(resp, 256); // 視儀器回應字串長度調整
    if (n <= 0) {
        m_lastError = QString("Read failed for: %1").arg(cmd);
        qWarning() << "[Instrument]" << m_lastError;
        return false;
    }
    result = QString(resp).trimmed();
    m_lastError.clear();
    return true;
}

bool InstrumentWithCommBase::queryBinary(const QString& cmd, QByteArray& out, int maxHeaderBytes)
{
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
    // 先讀 maxHeaderBytes（足夠容納 '#', <d>, 以及 <len> 的字元）
    int n = read(chunk, maxHeaderBytes);
    if (n <= 0) {
        m_lastError = QString("Read failed (header) for: %1").arg(cmd);
        qWarning() << "[Instrument]" << m_lastError;
        return false;
    }
    header.append(chunk);

    // 3) 基本檢查：必須是 binary block
    if (header.isEmpty() || header[0] != '#') {
        // 有些儀器可能直接回純文字；若你想嚴格限制，就當作錯誤
        m_lastError = QString("Unexpected response (not a binary block) for: %1").arg(cmd);
        qWarning() << "[Instrument]" << m_lastError << " resp head =" << header.left(16);
        return false;
    }

    if (header.size() < 2) {
        // header 還不完整，再讀到至少 2 bytes
        int need = 2 - header.size();
        if (need > 0) {
            int m = read(chunk, need);
            if (m <= 0) {
                m_lastError = "Read failed while completing binary header (ndig)";
                qWarning() << "[Instrument]" << m_lastError;
                return false;
            }
            header.append(chunk.left(m));
        }
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
        // #0 是不定長度（直到 EOI）；這裡先不支援
        m_lastError = "Indefinite-length block (#0) not supported";
        qWarning() << "[Instrument]" << m_lastError;
        return false;
    }

    // 5) 確保把 <len> 的字元讀齊
    int headerNeeded = 2 + ndig; // '#' + ndig + len(ndig digits)
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

    // 7) 把 header 後面在第一包中的「已經攜帶的 payload」先取出
    int alreadyPayload = header.size() - headerNeeded;
    if (alreadyPayload > 0) {
        out.append(header.constData() + headerNeeded, alreadyPayload);
    }

    // 8) 繼續把剩下的 payload 讀完
    int remain = payloadLen - alreadyPayload;
    while (remain > 0) {
        int chunkSize = qMin(remain, 64 * 1024); // 分塊讀，避免一次申請過大 buffer
        int m = read(chunk, chunkSize);
        if (m <= 0) {
            m_lastError = QString("Read failed while receiving payload, remain=%1").arg(remain);
            qWarning() << "[Instrument]" << m_lastError;
            return false;
        }
        out.append(chunk.constData(), m);
        remain -= m;
    }

    m_lastError.clear();
    return true;
}

void InstrumentWithCommBase::sendCommandWithLog(const QString& cmd, const QString& tag) {
    if (write(cmd) < 0) {
        m_lastError = QString("Write failed: %1").arg(cmd);
        qWarning() << (tag.isEmpty() ? "[Instrument]" : tag) << m_lastError;
    } else {
        m_lastError.clear();
    }
}
