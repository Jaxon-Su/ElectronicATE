// Unit-test VISA entry points only. The application still links the real NI-VISA library.
#include "tcpcommunication.h"
#include "gpibcommunication.h"
#include <cstring>
#include <QThread>
#include "transfercancellation.h"
#include <iostream>
#include <stdexcept>
static int call = 0;
static bool fail = true;
static bool clearFails = false, flushFails = false;
static int mode = 0;
static int timeout = 0;
static QByteArray message;
static qsizetype offset = 0;
static bool termcharEnabled = true;
static QAtomicInt cancelled = 0;
extern "C" {
ViStatus _VI_FUNC viOpenDefaultRM(ViPSession vi) { *vi = 1; return VI_SUCCESS; }
ViStatus _VI_FUNC viOpen(ViSession, ViConstRsrc, ViAccessMode, ViUInt32, ViPSession vi) { *vi = 2; return VI_SUCCESS; }
ViStatus _VI_FUNC viClose(ViObject) { return VI_SUCCESS; }
ViStatus _VI_FUNC viSetAttribute(ViObject, ViAttr attr, ViAttrState value) {
    if (attr == VI_ATTR_TMO_VALUE) timeout = int(value);
    if (attr == VI_ATTR_TERMCHAR_EN) termcharEnabled = value != VI_FALSE;
    return VI_SUCCESS;
}
ViStatus _VI_FUNC viClear(ViSession) { return clearFails ? VI_ERROR_TMO : VI_SUCCESS; }
ViStatus _VI_FUNC viFlush(ViSession, ViUInt16) { return flushFails ? VI_ERROR_TMO : VI_SUCCESS; }
ViStatus _VI_FUNC viWrite(ViSession, ViConstBuf, ViUInt32 cnt, ViPUInt32 ret) { *ret = cnt; return VI_SUCCESS; }
ViStatus _VI_FUNC viRead(ViSession, ViPBuf buf, ViUInt32 maximum, ViPUInt32 ret) {
    if (mode == 5) {
        const auto count = qMin(qsizetype(maximum), message.size() - offset);
        std::memcpy(buf, message.constData() + offset, size_t(count));
        offset += count; *ret = ViUInt32(count);
        return offset == message.size() ? VI_SUCCESS : VI_SUCCESS_MAX_CNT;
    }
    if (mode == 1) { QThread::msleep(15); *ret = 0; ++call; return VI_SUCCESS_MAX_CNT; }
    if (mode == 2) { *ret = 0; ++call; return VI_SUCCESS_MAX_CNT; }
    if (mode == 3) cancelled.storeRelease(1);
    if (mode == 4) QThread::msleep(6);
    std::memcpy(buf, "abc", 3); *ret = 3;
    if (++call == 1 || mode == 4) return VI_SUCCESS_MAX_CNT;
    return fail ? VI_ERROR_TMO : VI_SUCCESS;
}
}
int main() {
    TcpCommunication tcp("fake"); GpibCommunication gpib("fake");
    for (ICommunication* comm : {static_cast<ICommunication*>(&tcp), static_cast<ICommunication*>(&gpib)}) {
        termcharEnabled = true;
        if (!comm->open()) return 1;
        if (termcharEnabled) return 12;
        clearFails = true;
        if (comm->deviceClear() || !comm->lastError().contains("viClear")) return 9;
        clearFails = false; flushFails = true;
        if (comm->deviceClear() || !comm->lastError().contains("viFlush")) return 10;
        flushFails = false;
        if (!comm->deviceClear() || !comm->lastError().isEmpty()) return 11;
        QByteArray bytes;
        mode = 0; call = 0; fail = true;
        if (comm->readFully(bytes) || !bytes.isEmpty() || comm->lastError().isEmpty()) return 2;
        call = 0; fail = false;
        if (!comm->readFully(bytes) || bytes != "abcabc" || !comm->lastError().isEmpty()) return 3;
        comm->setTimeoutMs(10);
        for (int scenario : {1, 2, 4}) {
            mode = scenario; call = 0;
            if (comm->readFully(bytes) || !bytes.isEmpty() || timeout != 10 || call > 3) return 4;
        }
        mode = 0; call = 0; cancelled.storeRelease(1);
        {
            TransferCancellation guard(cancelled);
            if (comm->readFully(bytes) || call != 0 || !bytes.isEmpty()) return 5;
            cancelled.storeRelease(0); mode = 3;
            if (comm->readFully(bytes) || !bytes.isEmpty()) return 6;
        }
        // Cancellation does not leak beyond the capture scope or prevent cleanup writes.
        mode = 0; call = 0; comm->setTimeoutMs(10000);
        if (comm->write("OFF") != 3 || !comm->readFully(bytes)) return 7;
        // Real transport classes must read through embedded LF/NUL bytes to VISA END,
        // including when only the response LF remains in the next 64 KiB chunk.
        mode = 5;
        for (int length : {65535, 65536, 65537, 400128}) {
            for (const auto &suffix : {QByteArray(), QByteArray("\n")}) {
                message.resize(length);
                for (int i = 0; i < length; ++i) message[i] = char(i % 256);
                message += suffix; offset = 0;
                if (!comm->readFully(bytes) || bytes != message || offset != message.size()) return 13;
                if (timeout != 10000 || !comm->lastError().isEmpty()) return 14;
                // The next query must not see a leftover response LF.
                message = "0\n"; offset = 0;
                if (comm->write("*ESR?\n") != 6 || comm->read(bytes, 256) != 2 || bytes != "0\n") return 15;
            }
        }
        comm->close(); bytes = "stale";
        if (comm->readFully(bytes) || !bytes.isEmpty()) return 8;
    }
    std::cout << "PASS: TCP/GPIB partial timeout rejected; complete transfer retained\n";
}
