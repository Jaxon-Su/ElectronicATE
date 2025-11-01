#include "communicationfactory.h"
#include "gpibcommunication.h"
#include "tcpcommunication.h"
#include "serialcommunication.h"
#include <QRegularExpression>
#include <QString>

ICommunication* CommunicationFactory::create(const QString& resource)
{
    // 新增: 純數字也當作 GPIB
    static const QRegularExpression numberOnly("^[0-9]+$");
    if (numberOnly.match(resource).hasMatch())
        return new GpibCommunication("GPIB0::" + resource + "::INSTR");

    if (resource.startsWith("GPIB", Qt::CaseInsensitive))
        return new GpibCommunication(resource);

    static const QRegularExpression rx(
        "^TCPIP::([\\d\\.]+):(\\d+)",
        QRegularExpression::CaseInsensitiveOption
        );
    if (resource.startsWith("TCPIP", Qt::CaseInsensitive)) {
        QRegularExpressionMatch match = rx.match(resource);
        if (match.hasMatch())
            return new TcpCommunication(match.captured(1), match.captured(2).toUShort());
    }

    if (resource.startsWith("COM", Qt::CaseInsensitive) ||
        resource.startsWith("/dev/tty", Qt::CaseInsensitive))
        return new SerialCommunication(resource);

    return nullptr;
}


