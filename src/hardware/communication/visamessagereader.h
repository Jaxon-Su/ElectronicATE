#pragma once
#include "transfercancellation.h"
#include <visa.h>
#include <QByteArray>
#include <QElapsedTimer>
#include <QScopeGuard>
#include <QString>

namespace VisaMessageReader
{
// The configured timeout bounds the whole message, not each chunk separately.
inline bool read(ViSession session, int timeoutMs, QByteArray &data, QString &error)
{
    data.clear();
    auto fail = [&](const QString &reason) {
        data.clear();
        error = reason;
        return false;
    };
    if (timeoutMs <= 0)
        return fail("VISA: transfer timeout must be positive");
    QElapsedTimer elapsed;
    elapsed.start();
    auto restore = qScopeGuard([&] {
        viSetAttribute(session, VI_ATTR_TMO_VALUE, static_cast<ViAttrState>(timeoutMs));
    });
    constexpr ViUInt32 chunkSize = 64 * 1024;
    QByteArray chunk(chunkSize, Qt::Uninitialized);
    for (;;) {
        if (TransferCancellation::requested())
            return fail("VISA: transfer cancelled");
        const qint64 remaining = timeoutMs - elapsed.elapsed();
        if (remaining <= 0)
            return fail("VISA: message transfer deadline exceeded");
        if (viSetAttribute(session, VI_ATTR_TMO_VALUE, static_cast<ViAttrState>(remaining)) < VI_SUCCESS)
            return fail("VISA: unable to set transfer deadline");
        ViUInt32 count = 0;
        const ViStatus status = viRead(session, reinterpret_cast<ViBuf>(chunk.data()), chunkSize, &count);
        if (TransferCancellation::requested())
            return fail("VISA: transfer cancelled");
        if (elapsed.elapsed() >= timeoutMs)
            return fail("VISA: message transfer deadline exceeded");
        if ((status != VI_SUCCESS && status != VI_SUCCESS_MAX_CNT) || count > chunkSize)
            return fail(QString("VISA: incomplete or failed transfer, code=%1").arg(status));
        if (count)
            data.append(chunk.constData(), static_cast<qsizetype>(count));
        if (status == VI_SUCCESS) {
            if (data.isEmpty())
                return fail("VISA: empty transfer");
            if (viSetAttribute(session, VI_ATTR_TMO_VALUE, static_cast<ViAttrState>(timeoutMs)) < VI_SUCCESS)
                return fail("VISA: unable to restore transfer timeout");
            restore.dismiss();
            error.clear();
            return true;
        }
        if (!count)
            return fail("VISA: transfer made no progress");
    }
}
} // namespace VisaMessageReader
