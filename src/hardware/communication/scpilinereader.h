#pragma once
#include "icommunication.h"

// RS-232 can split a single SCPI reply anywhere, including inside "OFF" or "127.0".
// A failed frame invalidates subsequent readbacks until the port is reopened.
class ScpiLineReader {
  public:
    void reset() { m_synchronized = true; }
    int read(ICommunication &port, QByteArray &reply, int maxLen)
    {
        reply.clear();
        if (!m_synchronized || maxLen <= 0) return -1;
        m_synchronized = false;
        const int timeout = port.timeoutMs();
        QElapsedTimer timer; timer.start();
        const auto restore = qScopeGuard([&]{ port.setTimeoutMs(timeout); });
        QByteArray line;
        while (line.size() < maxLen) {
            const auto left = timeout - timer.elapsed();
            if (left <= 0 || TransferCancellation::requested()) return -1;
            port.setTimeoutMs(int(left));
            QByteArray chunk;
            const int room = maxLen - int(line.size());
            const int n = port.read(chunk, room);
            if (n <= 0 || n != chunk.size() || n > room || timer.elapsed() >= timeout
                || TransferCancellation::requested()) return -1;
            line += chunk;
            const auto lf = line.indexOf('\n');
            if (lf < 0) continue;
            if (lf != line.size() - 1) return -1; // unexpected second response
            reply = line;
            m_synchronized = true;
            return int(reply.size());
        }
        return -1;
    }
  private:
    bool m_synchronized = true;
};
