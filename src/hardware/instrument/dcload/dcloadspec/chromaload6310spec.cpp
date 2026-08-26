#include "chromaload6310spec.h"
#include <QMap>
#include <QString>
#include <functional>
#include <optional>
#include <QDebug>

// ==================== 規格映射表 ====================

static QMap<QString, std::function<ChromaLoadSpec()>> createSpecMap = {
    { "63101", createChroma63101Spec },
    { "63102", createChroma63102Spec },
    { "63103", createChroma63103Spec },
    { "63105", createChroma63105Spec },
    { "63106", createChroma63106Spec },
    { "63108", createChroma63108Spec },
    { "63112", createChroma63112Spec },
    };

// ==================== findPowerRange ====================

std::optional<PowerRangeSpec> findPowerRange(const QString& subModel, double currval)
{
    if (!createSpecMap.contains(subModel)) {
        qWarning() << "[Chroma6310Spec] Unknown subModel:" << subModel;
        return std::nullopt;
    }

    ChromaLoadSpec spec = createSpecMap[subModel]();

    for (const auto& range : spec.ranges) {
        if (currval <= range.currentSpec.maxCurrent) {
            return range;
        }
    }

    // 超出最大電流範圍 → 回傳最後一個範圍（CCH），讓硬體自行保護
    if (!spec.ranges.isEmpty()) {
        qWarning() << "[Chroma6310Spec] Current" << currval
                   << "A exceeds all ranges for" << subModel
                   << "- using CCH (last range)";
        return spec.ranges.last();
    }

    return std::nullopt;
}

// ==================== selectOptimalLoadMode ====================

QString selectOptimalLoadMode(const QString& subModel,
                              double current,
                              double voltage)
{
    const double MARGIN = 0.95;
    double expectedPower = current * voltage;

    // Step 1：找到適合電流的範圍
    auto rangeOpt = findPowerRange(subModel, current / MARGIN);
    if (!rangeOpt) {
        qWarning() << "[Chroma6310Spec] Unknown subModel:" << subModel << "- fallback CCH";
        return "CCH";
    }

    const PowerRangeSpec& range = *rangeOpt;

    // Step 2：驗證功率與電壓是否在規格內（95% margin）
    bool currentOK = (current <= range.currentSpec.maxCurrent * MARGIN);
    bool powerOK   = (expectedPower <= range.power * MARGIN);
    bool voltageOK = (voltage >= range.voltageSpec.minVoltage &&
                      voltage <= range.voltageSpec.maxVoltage);

    if (!currentOK || !powerOK || !voltageOK) {
        qWarning() << "[Chroma6310Spec] Range" << range.mode
                   << "found but constraints not met:"
                   << "currentOK=" << currentOK
                   << "powerOK="   << powerOK
                   << "voltageOK=" << voltageOK
                   << "- fallback CCH";
        return "CCH";
    }

    // Step 3：直接回傳 mode 欄位（不依賴 remark 解析）
    qDebug() << "[Chroma6310Spec] Selected mode:" << range.mode
             << "SubModel:" << subModel
             << "I=" << current << "A"
             << "V=" << voltage << "V"
             << "P=" << expectedPower << "W";

    return range.mode;
}

// ==================== 63101 規格 ====================

ChromaLoadSpec createChroma63101Spec()
{
    ChromaLoadSpec spec;
    spec.model = "63101";

    // 20W Range (CCL)
    PowerRangeSpec range20;
    range20.mode  = "CCL";
    range20.power = 20.0;
    range20.currentSpec = {
        0.0, 4.0, 0.001,
        {0.0, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCL (低檔)"
    };
    range20.voltageSpec = {
        1.0, 80.0, 0.02,
        {0.0, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "1~80V"
    };
    range20.resistanceSpec = {
        {
            {0.0375, 150.0, {0.0, 0.2, 0.1, "V", "150Ω : 0.1V+0.2%"}},
            {1.875, 7500.0, {0.0, 0.1, 0.01, "V", "7.5KΩ : 0.01V+0.1%"}}
        },
        12, ""
    };
    range20.dynamicSpec = {
        0.000025, 0.01, 0.000001,
        0.00064, 0.16, 0.00064,
        0.000001,
        0.0, 4.0, 0.001, 0.004,
        "T1&T2: 0.025ms~10ms; Slew:0.64~160mA/us; Current:0~4A"
    };
    range20.remark = "20W/CCL";

    // 200W Range (CCH)
    PowerRangeSpec range200;
    range200.mode  = "CCH";
    range200.power = 200.0;
    range200.currentSpec = {
        0.0, 40.0, 0.01,
        {0.0, 0.2, 0.0, "%F.S.", "0.1%+0.2%F.S."},
        "CCH (高檔)"
    };
    range200.voltageSpec = {
        1.0, 80.0, 0.02,
        {0.0, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "1~80V"
    };
    range200.resistanceSpec = {
        {
            {0.0375, 150.0, {0.0, 0.2, 0.1, "V", "150Ω : 0.1V+0.2%"}},
            {1.875, 7500.0, {0.0, 0.1, 0.01, "V", "7.5KΩ : 0.01V+0.1%"}}
        },
        12, ""
    };
    range200.dynamicSpec = {
        0.000025, 0.01, 0.000001,
        0.0064, 1.6, 0.0064,
        0.000001,
        0.0, 40.0, 0.01, 0.004,
        "T1&T2: 0.025ms~10ms; Slew:6.4~1600mA/us; Current:0~40A"
    };
    range200.remark = "200W/CCH";

    spec.ranges = { range20, range200 };
    return spec;
}

// ==================== 63102 規格 ====================

ChromaLoadSpec createChroma63102Spec()
{
    ChromaLoadSpec spec;
    spec.model = "63102";

    PowerRangeSpec range20;
    range20.mode  = "CCL";
    range20.power = 20.0;
    range20.currentSpec = {
        0.0, 2.0, 0.0005,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "單通道 20W"
    };
    range20.voltageSpec = {
        1.0, 80.0, 0.02,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "1~80V"
    };
    range20.resistanceSpec = {
        {
            {0.075, 300.0, {0.1, 0.2, 0.0, "V", "300Ω : 0.1V+0.2%"}},
            {3.75, 15000.0, {0.01, 0.1, 0.0, "V", "15KΩ : 0.01V+0.1%"}}
        },
        12, ""
    };
    range20.dynamicSpec = {
        0.000025, 0.01, 0.000001,
        0.00032, 0.08, 0.00032,
        0.000001,
        0.0, 2.0, 0.0005, 0.004,
        "T1&T2: 0.025ms~10ms; Slew:0.32~80mA/us; Current:0~2A"
    };
    range20.remark = "20W/CCL";

    PowerRangeSpec range100;
    range100.mode  = "CCH";
    range100.power = 100.0;
    range100.currentSpec = {
        0.0, 20.0, 0.005,
        {0.1, 0.2, 0.0, "%F.S.", "0.1%+0.2%F.S."},
        "單通道 100W"
    };
    range100.voltageSpec = {
        1.0, 80.0, 0.02,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "1~80V"
    };
    range100.resistanceSpec = {
        {
            {0.075, 300.0, {0.1, 0.2, 0.0, "V", "300Ω : 0.1V+0.2%"}},
            {3.75, 15000.0, {0.01, 0.1, 0.0, "V", "15KΩ : 0.01V+0.1%"}}
        },
        12, ""
    };
    range100.dynamicSpec = {
        0.000025, 0.01, 0.000001,
        0.0032, 0.8, 0.0032,
        0.000001,
        0.0, 20.0, 0.005, 0.004,
        "T1&T2: 0.025ms~10ms; Slew:3.2~800mA/us; Current:0~20A"
    };
    range100.remark = "100W/CCH";

    spec.ranges = { range20, range100 };
    return spec;
}

// ==================== 63103 規格 ====================

ChromaLoadSpec createChroma63103Spec()
{
    ChromaLoadSpec spec;
    spec.model = "63103";

    // 30W Range (CCL)
    PowerRangeSpec range30;
    range30.mode  = "CCL";
    range30.power = 30.0;
    range30.currentSpec = {
        0.0, 6.0, 0.0015,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCL (低檔)"
    };
    range30.voltageSpec = {
        1.0, 80.0, 0.02,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "1~80V"
    };
    range30.resistanceSpec = {
        {
            {0.025, 100.0, {0.1, 0.2, 0.0, "V", "100Ω : 0.1V+0.2%"}},
            {1.25, 5000.0, {0.01, 0.1, 0.0, "V", "5KΩ : 0.01V+0.1%"}}
        },
        12, ""
    };
    range30.dynamicSpec = {
        0.000025, 0.01, 0.000001,
        0.001, 0.25, 0.001,
        0.000001,
        0.0, 6.0, 0.0015, 0.004,
        "T1&T2: 0.025ms~10ms; Slew:1~250mA/us; Current:0~6A"
    };
    range30.remark = "30W/CCL";

    // 300W Range (CCH)
    PowerRangeSpec range300;
    range300.mode  = "CCH";
    range300.power = 300.0;
    range300.currentSpec = {
        0.0, 60.0, 0.015,
        {0.1, 0.2, 0.0, "%F.S.", "0.1%+0.2%F.S."},
        "CCH (高檔)"
    };
    range300.voltageSpec = {
        2.5, 500.0, 0.02,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "2.5~500V"
    };
    range300.resistanceSpec = {
        {
            {0.025, 100.0, {0.1, 0.2, 0.0, "V", "100Ω : 0.1V+0.2%"}},
            {1.25, 5000.0, {0.01, 0.1, 0.0, "V", "5KΩ : 0.01V+0.1%"}}
        },
        12, ""
    };
    range300.dynamicSpec = {
        0.000025, 0.01, 0.000001,
        0.01, 2.5, 0.01,
        0.000001,
        0.0, 60.0, 0.015, 0.004,
        "T1&T2: 0.025ms~10ms; Slew:10~2500mA/us; Current:0~60A"
    };
    range300.remark = "300W/CCH";

    spec.ranges = { range30, range300 };
    return spec;
}

// ==================== 63105 規格 ====================

ChromaLoadSpec createChroma63105Spec()
{
    ChromaLoadSpec spec;
    spec.model = "63105";

    // 30W Range (CCL)
    PowerRangeSpec range30;
    range30.mode  = "CCL";
    range30.power = 30.0;
    range30.currentSpec = {
        0.0, 1.0, 0.00025,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCL (低檔)"
    };
    range30.voltageSpec = {
        2.5, 500.0, 0.125,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "2.5~500V"
    };
    range30.resistanceSpec = {
        {
            {1.25, 5000.0, {0.02, 0.2, 0.0, "V", "5KΩ : 20mV+0.2%"}},
            {50.0, 200000.0, {0.005, 0.1, 0.0, "V", "200KΩ : 5mV+0.1%"}}
        },
        12, ""
    };
    range30.dynamicSpec = {
        0.000025, 0.01, 0.000001,
        0.00016, 0.04, 0.00016,
        0.000001,
        0.0, 1.0, 0.00025, 0.004,
        "T1&T2: 0.025ms~10ms; Slew:0.16~40mA/us; Current:0~1A"
    };
    range30.remark = "30W/CCL";

    // 300W Range (CCH)
    PowerRangeSpec range300;
    range300.mode  = "CCH";
    range300.power = 300.0;
    range300.currentSpec = {
        0.0, 10.0, 0.0025,
        {0.1, 0.2, 0.0, "%F.S.", "0.1%+0.2%F.S."},
        "CCH (高檔)"
    };
    range300.voltageSpec = {
        2.5, 500.0, 0.125,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "2.5~500V"
    };
    range300.resistanceSpec = {
        {
            {1.25, 5000.0, {0.02, 0.2, 0.0, "V", "5KΩ : 20mV+0.2%"}},
            {50.0, 200000.0, {0.005, 0.1, 0.0, "V", "200KΩ : 5mV+0.1%"}}
        },
        12, ""
    };
    range300.dynamicSpec = {
        0.000025, 0.01, 0.000001,
        0.0016, 0.4, 0.0016,
        0.000001,
        0.0, 10.0, 0.0025, 0.004,
        "T1&T2: 0.025ms~10ms; Slew:1.6~400mA/us; Current:0~10A"
    };
    range300.remark = "300W/CCH";

    spec.ranges = { range30, range300 };
    return spec;
}

// ==================== 63106 規格 ====================

ChromaLoadSpec createChroma63106Spec()
{
    ChromaLoadSpec spec;
    spec.model = "63106";

    // 60W Range (CCL)
    PowerRangeSpec range60;
    range60.mode  = "CCL";
    range60.power = 60.0;
    range60.currentSpec = {
        0.0, 12.0, 0.003,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCL (低檔)"
    };
    range60.voltageSpec = {
        1.0, 80.0, 0.02,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "1~80V"
    };
    range60.resistanceSpec = {
        {
            {12.5e-3, 50.0, {0.1, 0.5, 0.0, "V", "50Ω : 0.1V+0.5%"}},
            {0.625, 2500.0, {0.01, 0.2, 0.0, "V", "2.5KΩ : 0.01V+0.2%"}}
        },
        12, ""
    };
    range60.dynamicSpec = {
        0.000025, 0.01, 0.000001,
        0.002, 0.5, 0.002,
        0.000001,
        0.0, 12.0, 0.003, 0.004,
        "T1&T2: 0.025ms~10ms; Slew:2~500mA/us; Current:0~12A"
    };
    range60.remark = "60W/CCL";

    // 600W Range (CCH)
    PowerRangeSpec range600;
    range600.mode  = "CCH";
    range600.power = 600.0;
    range600.currentSpec = {
        0.0, 120.0, 0.03,
        {0.1, 0.2, 0.0, "%F.S.", "0.1%+0.2%F.S."},
        "CCH (高檔)"
    };
    range600.voltageSpec = {
        1.0, 80.0, 0.02,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "1~80V"
    };
    range600.resistanceSpec = {
        {
            {12.5e-3, 50.0, {0.1, 0.5, 0.0, "V", "50Ω : 0.1V+0.5%"}},
            {0.625, 2500.0, {0.01, 0.2, 0.0, "V", "2.5KΩ : 0.01V+0.2%"}}
        },
        12, ""
    };
    range600.dynamicSpec = {
        0.000025, 0.01, 0.000001,
        0.02, 5.0, 0.02,
        0.000001,
        0.0, 120.0, 0.03, 0.004,
        "T1&T2: 0.025ms~10ms; Slew:20~5000mA/us; Current:0~120A"
    };
    range600.remark = "600W/CCH";

    spec.ranges = { range60, range600 };
    return spec;
}

// ==================== 63108 規格 ====================

ChromaLoadSpec createChroma63108Spec()
{
    ChromaLoadSpec spec;
    spec.model = "63108";

    // 60W Range (CCL)
    PowerRangeSpec range60;
    range60.mode  = "CCL";
    range60.power = 60.0;
    range60.currentSpec = {
        0.0, 2.0, 0.0005,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCL (低檔)"
    };
    range60.voltageSpec = {
        2.5, 500.0, 0.125,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "2.5~500V"
    };
    range60.resistanceSpec = {
        {
            {0.625, 2500.0, {0.05, 0.2, 0.0, "V", "2.5KΩ : 50mV+0.2%"}},
            {25.0, 100000.0, {0.005, 0.1, 0.0, "V", "100KΩ : 5mV+0.1%"}}
        },
        12, ""
    };
    range60.dynamicSpec = {
        0.000025, 0.01, 0.000001,
        0.00032, 0.08, 0.00032,
        0.000001,
        0.0, 2.0, 0.0005, 0.004,
        "T1&T2: 0.025ms~10ms; Slew:0.32~80mA/us; Current:0~2A"
    };
    range60.remark = "60W/CCL";

    // 600W Range (CCH)
    PowerRangeSpec range600;
    range600.mode  = "CCH";
    range600.power = 600.0;
    range600.currentSpec = {
        0.0, 20.0, 0.005,
        {0.1, 0.2, 0.0, "%F.S.", "0.1%+0.2%F.S."},
        "CCH (高檔)"
    };
    range600.voltageSpec = {
        2.5, 500.0, 0.125,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "2.5~500V"
    };
    range600.resistanceSpec = {
        {
            {0.625, 2500.0, {0.05, 0.2, 0.0, "V", "2.5KΩ : 50mV+0.2%"}},
            {25.0, 100000.0, {0.005, 0.1, 0.0, "V", "100KΩ : 5mV+0.1%"}}
        },
        12, ""
    };
    range600.dynamicSpec = {
        0.000025, 0.01, 0.000001,
        0.0032, 0.8, 0.0032,
        0.000001,
        0.0, 20.0, 0.005, 0.004,
        "T1&T2: 0.025ms~10ms; Slew:3.2~800mA/us; Current:0~20A"
    };
    range600.remark = "600W/CCH";

    spec.ranges = { range60, range600 };
    return spec;
}

// ==================== 63112 規格 ====================

ChromaLoadSpec createChroma63112Spec()
{
    ChromaLoadSpec spec;
    spec.model = "63112";

    // 120W Range (CCL)
    PowerRangeSpec range120;
    range120.mode  = "CCL";
    range120.power = 120.0;
    range120.currentSpec = {
        0.0, 24.0, 0.006,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCL (低檔)"
    };
    range120.voltageSpec = {
        1.0, 80.0, 0.02,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "1~80V"
    };
    range120.resistanceSpec = {
        {
            {6.25e-3, 2.0, {0.1, 0.8, 0.0, "V", "25Ω : 0.1V+0.8%"}},
            {0.3125, 1250.0, {0.01, 0.2, 0.0, "V", "1.25KΩ : 0.01V+0.2%"}}
        },
        12, ""
    };
    range120.dynamicSpec = {
        0.000025, 0.01, 0.000001,
        0.000004, 0.001, 0.000004,
        0.000001,
        0.0, 24.0, 0.006, 0.004,
        "T1&T2: 0.025ms~10ms; Slew:4~1000mA/us; Current:0~24A"
    };
    range120.remark = "120W/CCL";

    // 1200W Range (CCH)
    PowerRangeSpec range1200;
    range1200.mode  = "CCH";
    range1200.power = 1200.0;
    range1200.currentSpec = {
        0.0, 240.0, 0.06,
        {0.1, 0.2, 0.0, "%F.S.", "0.1%+0.2%F.S."},
        "CCH (高檔)"
    };
    range1200.voltageSpec = {
        1.0, 80.0, 0.02,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "1~80V"
    };
    range1200.resistanceSpec = {
        {
            {6.25e-3, 2.0, {0.1, 0.8, 0.0, "V", "25Ω : 0.1V+0.8%"}},
            {0.3125, 1250.0, {0.01, 0.2, 0.0, "V", "1.25KΩ : 0.01V+0.2%"}}
        },
        12, ""
    };
    range1200.dynamicSpec = {
        0.000025, 0.01, 0.000001,
        0.00004, 0.01, 0.00004,
        0.000001,
        0.0, 240.0, 0.06, 0.004,
        "T1&T2: 0.025ms~10ms; Slew:40~10000mA/us; Current:0~240A"
    };
    range1200.remark = "1200W/CCH";

    spec.ranges = { range120, range1200 };
    return spec;
}
