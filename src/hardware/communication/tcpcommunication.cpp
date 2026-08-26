#include "tcpcommunication.h"
#include <QDebug>

TcpCommunication::TcpCommunication(const QString& resource, int timeoutMs)
    : m_resource(resource)
    , m_timeoutMs(timeoutMs)
{}

TcpCommunication::~TcpCommunication()
{
    if (m_instr) {
        viClose(m_instr);
        m_instr = 0;
    }
    if (m_rm) {
        viClose(m_rm);
        m_rm = 0;
    }
    m_opened = false;
}

bool TcpCommunication::open()
{
    if (m_opened) return true;

    // 1. 打開資源管理器
    if (viOpenDefaultRM(&m_rm) != VI_SUCCESS) {
        m_error = "VISA: viOpenDefaultRM failed";
        return false;
    }

    // 2. 打開儀器（主要支援 VXI-11 "TCPIP0::x.x.x.x::INSTR"）
    ViStatus status = viOpen(m_rm,
                             m_resource.toUtf8().constData(),
                             VI_NULL, VI_NULL,
                             &m_instr);
    if (status != VI_SUCCESS) {
        m_error = QString("VISA: Open failed [%1] code=%2").arg(m_resource).arg(status);
        viClose(m_rm);
        m_rm = 0;
        return false;
    }

    // 3. 套用逾時設定
    viSetAttribute(m_instr, VI_ATTR_TMO_VALUE, static_cast<ViUInt32>(m_timeoutMs));

    // 4. 關閉 termchar，避免 VISA 把資料中的 \n 誤判為 EOI 提前中止讀取
    viSetAttribute(m_instr, VI_ATTR_TERMCHAR_EN, VI_FALSE);

    m_error.clear();
    m_opened = true;
    qDebug() << "[TcpComm] Opened:" << m_resource << "timeout=" << m_timeoutMs << "ms";
    return true;
}

void TcpCommunication::close()
{
    if (m_instr) {
        viClose(m_instr);
        m_instr = 0;
    }
    if (m_rm) {
        viClose(m_rm);
        m_rm = 0;
    }
    m_opened = false;
}

int TcpCommunication::write(const QByteArray& data)
{
    if (!m_opened) {
        m_error = "VISA: Not opened";
        return -1;
    }

    ViUInt32 written = 0;
    ViStatus status  = viWrite(m_instr,
                              reinterpret_cast<ViBuf>(const_cast<char*>(data.constData())),
                              static_cast<ViUInt32>(data.size()),
                              &written);

    if (status < VI_SUCCESS) {
        m_error = QString("VISA: Write failed, code=%1").arg(status);
        return -1;
    }
    m_error.clear();
    return static_cast<int>(written);
}

int TcpCommunication::read(QByteArray& data, int maxLen)
{
    if (!m_opened) {
        m_error = "VISA: Not opened";
        return -1;
    }

    QByteArray buf(maxLen, Qt::Uninitialized);
    ViUInt32 retCount = 0;
    ViStatus status   = viRead(m_instr,
                             reinterpret_cast<ViBuf>(buf.data()),
                             static_cast<ViUInt32>(maxLen),
                             &retCount);

    if (status == VI_SUCCESS || status == VI_SUCCESS_MAX_CNT) {
        // VI_SUCCESS_MAX_CNT 表示緩衝區滿但仍有更多資料，呼叫端需繼續讀。
        data = buf.left(static_cast<int>(retCount));
        m_error.clear();
        return static_cast<int>(retCount);
    }

    data.clear();
    m_error = QString("VISA: Read failed, code=0x%1").arg(
        static_cast<uint>(status), 8, 16, QChar('0'));
    return -1;
}

bool TcpCommunication::isOpen() const
{
    return m_opened;
}

bool TcpCommunication::readFully(QByteArray& data)
{
    if (!m_opened) {
        m_error = "VISA: Not opened";
        return false;
    }

    data.clear();
    static const int CHUNK_SIZE = 64 * 1024;
    QByteArray buf(CHUNK_SIZE, Qt::Uninitialized);
    ViUInt32 retCount = 0;
    ViStatus status;

    // VXI-11 狀態碼：
    //   VI_SUCCESS         = END 收到，傳輸正常結束
    //   VI_SUCCESS_MAX_CNT = 緩衝區滿但無 END，繼續讀
    do {
        status = viRead(m_instr,
                        reinterpret_cast<ViBuf>(buf.data()),
                        static_cast<ViUInt32>(CHUNK_SIZE),
                        &retCount);
        if (retCount > 0)
            data.append(buf.constData(), static_cast<int>(retCount));
    } while (status == VI_SUCCESS_MAX_CNT);

    if (data.isEmpty()) {
        m_error = QString("VISA: readFully got no data, code=0x%1")
                      .arg(static_cast<uint>(status), 8, 16, QChar('0'));
        return false;
    }

    m_error.clear();
    return true;
}

void TcpCommunication::setTimeoutMs(int ms)
{
    m_timeoutMs = ms;
    if (m_opened && m_instr) {
        viSetAttribute(m_instr, VI_ATTR_TMO_VALUE, static_cast<ViUInt32>(ms));
        qDebug() << "[TcpComm] Timeout updated to" << ms << "ms";
    }
}

bool TcpCommunication::deviceClear()
{
    if (!m_opened || !m_instr) {
        m_error = "VISA: deviceClear called but not opened";
        return false;
    }

    // ── 步驟 1：viClear 送出 VXI-11 Device Clear
    //    清空「儀器端」的 input queue 與 output buffer
    ViStatus status = viClear(m_instr);
    if (status < VI_SUCCESS) {
        m_error = QString("VISA: viClear failed, code=0x%1").arg(
            static_cast<uint>(status), 8, 16, QChar('0'));
        qWarning() << "[TcpComm] deviceClear failed:" << m_error;
        return false;
    }

    // ── 步驟 2：viFlush 丟棄「VISA 軟體端」的 receive buffer
    //    viClear 只清儀器端；若上次讀取中途中斷，NI-VISA 的
    //    formatted I/O buffer（VI_READ_BUF）與底層 I/O buffer
    //    （VI_IO_IN_BUF）可能仍有殘留資料，下次 queryBinary 會
    //    讀到舊資料開頭導致 binary block 解析失敗。
    //    VI_READ_BUF_DISCARD    : 清 formatted I/O read buffer
    //    VI_IO_IN_BUF_DISCARD   : 清底層 I/O input buffer
    viFlush(m_instr, VI_READ_BUF_DISCARD | VI_IO_IN_BUF_DISCARD);

    qDebug() << "[TcpComm] deviceClear OK (viClear + viFlush)";
    return true;
}
