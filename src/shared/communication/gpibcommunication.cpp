#include "gpibcommunication.h"
#include <QDebug>

GpibCommunication::GpibCommunication(const QString& resource)
    : m_resource(resource) {}

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
    if (viOpen(m_rm, m_resource.toUtf8().constData(), VI_NULL, VI_NULL, &m_instr) != VI_SUCCESS) {
        m_error = QString("GPIB: viOpen failed [%1]").arg(m_resource);
        viClose(m_rm);
        m_rm = 0;
        return false;
    }
    m_error.clear();
    m_opened = true;
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
    ViStatus st = viWrite(m_instr, (ViBuf)data.constData(), data.size(), &written);
    if (st != VI_SUCCESS) {
        m_error = QString("GPIB viWrite failed, status=%1").arg(st);
        return -1;
    }
    m_error.clear();
    return written;
}

int GpibCommunication::read(QByteArray& data, int maxLen) {
    if (!m_opened) {
        m_error = "GPIB not opened";
        return -1;
    }
    QByteArray buf(maxLen, Qt::Uninitialized);
    ViUInt32 retCount = 0;
    ViStatus st = viRead(m_instr, (ViBuf)buf.data(), maxLen, &retCount);
    if (st == VI_SUCCESS || st == VI_SUCCESS_MAX_CNT) {
        data = buf.left(retCount);
        m_error.clear(); // 清空舊錯誤
        return retCount;
    }
    data.clear();
    m_error = QString("GPIB viRead failed, status=%1").arg(st); // <== 寫入最新錯誤
    return -1;
}

bool GpibCommunication::isOpen() const {
    return m_opened;
}
