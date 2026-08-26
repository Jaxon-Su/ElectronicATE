#pragma once
#include <QStringList>

// 共用 Header 產生工具 — 消除 Page2VM / Page3VM 中完全相同的 for 迴圈
namespace TableHeaderBuilder {

    // ["Output", "Index1", ..., "IndexN"]
    inline QStringList buildIndexHeaders(int count)
    {
        QStringList h{"Output"};
        for (int i = 1; i <= count; ++i)
            h << QString("Index%1").arg(i);
        return h;
    }

    // ["Relay", "Index1", ..., "IndexN"]
    inline QStringList buildRelayHeaders(int count)
    {
        QStringList h{"Relay"};
        for (int i = 1; i <= count; ++i)
            h << QString("Index%1").arg(i);
        return h;
    }
}
