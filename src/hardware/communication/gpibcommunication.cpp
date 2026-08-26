#include "gpibcommunication.h"
#include <QDebug>

GpibCommunication::GpibCommunication(const QString& resource, int timeoutMs)
    : m_resource(resource)
    , m_timeoutMs(timeoutMs)
{}

GpibCommunication::~GpibCommunication() {
    if (m_instr) viClose(m_instr);
    if (m_rm) viClose(m_rm);
    m_instr = 0;
    m_rm = 0;
    m_opened = false;
}

bool GpibCommunication::open() {
    if (m_opened) {
        m_error.clear();
        return true;
    }
    if (viOpenDefaultRM(&m_rm) != VI_SUCCESS) {
        m_error = "GPIB: viOpenDefaultRM failed";
        return false;
    }
    ViStatus status = viOpen(m_rm, m_resource.toUtf8().constData(), VI_NULL, VI_NULL, &m_instr);
    if (status != VI_SUCCESS) {
        m_error = QString("GPIB: viOpen failed [%1] code=%2").arg(m_resource).arg(status);
        viClose(m_rm);
        m_rm = 0;
        return false;
    }

    // 套用逾時設定
    viSetAttribute(m_instr, VI_ATTR_TMO_VALUE, static_cast<ViUInt32>(m_timeoutMs));

    // 關閉 termchar，避免 VISA 把資料中的 \n 誤判為 EOI 提前中止讀取
    viSetAttribute(m_instr, VI_ATTR_TERMCHAR_EN, VI_FALSE);

    m_error.clear();
    m_opened = true;
    qDebug() << "[GpibComm] Opened:" << m_resource << "timeout=" << m_timeoutMs << "ms";
    return true;
}

void GpibCommunication::close() {
    bool err = false;
    if (m_instr && viClose(m_instr) != VI_SUCCESS) {
        m_error = "GPIB: viClose (instr) failed";
        err = true;
    }
    if (m_rm && viClose(m_rm) != VI_SUCCESS) {
        m_error = "GPIB: viClose (rm) failed";
        err = true;
    }
    m_instr = 0;
    m_rm = 0;
    m_opened = false;
    if (!err) m_error.clear();
}

int GpibCommunication::write(const QByteArray& data) {
    if (!m_opened) {
        m_error = "GPIB not opened";
        return -1;
    }
    ViUInt32 written = 0;
    ViStatus st = viWrite(m_instr,
                          reinterpret_cast<ViBuf>(const_cast<char*>(data.constData())),
                          static_cast<ViUInt32>(data.size()),
                          &written);
    if (st < VI_SUCCESS) {
        m_error = QString("GPIB: viWrite failed, code=%1").arg(st);
        return -1;
    }
    m_error.clear();
    return static_cast<int>(written);
}

int GpibCommunication::read(QByteArray& data, int maxLen) {
    if (!m_opened) {
        m_error = "GPIB not opened";
        return -1;
    }
    QByteArray buf(maxLen, Qt::Uninitialized);
    ViUInt32 retCount = 0;
    ViStatus st = viRead(m_instr,
                         reinterpret_cast<ViBuf>(buf.data()),
                         static_cast<ViUInt32>(maxLen),
                         &retCount);
    if (st == VI_SUCCESS || st == VI_SUCCESS_MAX_CNT) {
        data = buf.left(static_cast<int>(retCount));
        m_error.clear();
        return static_cast<int>(retCount);
    }
    data.clear();
    m_error = QString("GPIB: viRead failed, code=0x%1").arg(
        static_cast<uint>(st), 8, 16, QChar('0'));
    return -1;
}

bool GpibCommunication::isOpen() const {
    return m_opened;
}

bool GpibCommunication::readFully(QByteArray& data)
{
    if (!m_opened) {
        m_error = "GPIB: Not opened";
        return false;
    }

    data.clear();
    static const int CHUNK_SIZE = 64 * 1024;
    QByteArray buf(CHUNK_SIZE, Qt::Uninitialized);
    ViUInt32 retCount = 0;
    ViStatus status;

    // GPIB 狀態碼：
    //   VI_SUCCESS         = EOI 收到（硬體 EOI 線拉低），傳輸正常結束
    //   VI_SUCCESS_MAX_CNT = 緩衝區滿但無 EOI，繼續讀
    do {
        status = viRead(m_instr,
                        reinterpret_cast<ViBuf>(buf.data()),
                        static_cast<ViUInt32>(CHUNK_SIZE),
                        &retCount);
        if (retCount > 0)
            data.append(buf.constData(), static_cast<int>(retCount));
    } while (status == VI_SUCCESS_MAX_CNT);

    if (data.isEmpty()) {
        m_error = QString("GPIB: readFully got no data, code=0x%1")
                      .arg(static_cast<uint>(status), 8, 16, QChar('0'));
        return false;
    }

    m_error.clear();
    return true;
}

void GpibCommunication::setTimeoutMs(int ms)
{
    m_timeoutMs = ms;
    if (m_opened && m_instr) {
        viSetAttribute(m_instr, VI_ATTR_TMO_VALUE, static_cast<ViUInt32>(ms));
        qDebug() << "[GpibComm] Timeout updated to" << ms << "ms";
    }
}

bool GpibCommunication::deviceClear()
{
    if (!m_opened || !m_instr) {
        m_error = "GPIB: deviceClear called but not opened";
        return false;
    }

    // ── 步驟 1：viClear 送出 GPIB Device Clear (DCL/SDC)
    //    清空「儀器端」的 input queue 與 output buffer
    ViStatus status = viClear(m_instr);
    if (status < VI_SUCCESS) {
        m_error = QString("GPIB: viClear failed, code=0x%1").arg(
            static_cast<uint>(status), 8, 16, QChar('0'));
        qWarning() << "[GpibComm] deviceClear failed:" << m_error;
        return false;
    }

    // ── 步驟 2：viFlush 丟棄「VISA 軟體端」的 receive buffer
    //    VI_READ_BUF_DISCARD    : 清 formatted I/O read buffer
    //    VI_IO_IN_BUF_DISCARD   : 清底層 I/O input buffer
    viFlush(m_instr, VI_READ_BUF_DISCARD | VI_IO_IN_BUF_DISCARD);

    qDebug() << "[GpibComm] deviceClear OK (viClear + viFlush)";
    return true;
}
