#pragma once
#include <QString>
#include <QVariantMap>

struct DutRowData {
    bool        active   = true;
    QString     item;
    QString     ext;
    QString     retry    = "0";
    bool        report   = true;
    // ★ Dialog 設定（delay/osc/turnOn/turnOff/relay）
    //   key = 設定類型，value = 該 Dialog 的完整 QVariantMap
    //   例：settings["delay"] = { "delay_ms": 5000 }
    //       settings["osc"]   = { "timescale": "200ms", "ch1_enabled": true, ... }
    QVariantMap settings;
};
