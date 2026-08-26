#include "chromaload6310aspec.h"
#include <QMap>
#include <QString>
#include <functional>
#include <optional>
#include <QDebug>

// ==================== 規格映射表 ====================

static QMap<QString, std::function<ChromaLoadSpec()>> createSpecMap6310A = {
    { "63101A", createChroma63101ASpec },
    { "63102A", createChroma63102ASpec },
    { "63103A", createChroma63103ASpec },
    { "63105A", createChroma63105ASpec },
    { "63106A", createChroma63106ASpec },
    { "63107A", createChroma63107ASpec },
    { "63108A", createChroma63108ASpec },
    { "63110A", createChroma63110ASpec },
    { "63112A", createChroma63112ASpec },
    { "63113A", createChroma63113ASpec },
    { "63115A", createChroma63115ASpec },
    { "63123A", createChroma63123ASpec },
    };

// ==================== findPowerRange6310A ====================

std::optional<PowerRangeSpec> findPowerRange6310A(const QString& subModel, double currval)
{
    if (!createSpecMap6310A.contains(subModel)) {
        qWarning() << "[Chroma6310ASpec] Unknown subModel:" << subModel;
        return std::nullopt;
    }

    ChromaLoadSpec spec = createSpecMap6310A[subModel]();

    for (const auto& range : spec.ranges) {
        if (currval <= range.currentSpec.maxCurrent) {
            return range;
        }
    }

    // 超出最大電流範圍 → 回傳最後一個範圍（CCH），讓硬體自行保護
    if (!spec.ranges.isEmpty()) {
        qWarning() << "[Chroma6310ASpec] Current" << currval
                   << "A exceeds all ranges for" << subModel
                   << "- using CCH (last range)";
        return spec.ranges.last();
    }

    return std::nullopt;
}

// ==================== selectOptimalLoadMode6310A ====================

QString selectOptimalLoadMode6310A(const QString& subModel,
                                   double current,
                                   double voltage)
{
    const double MARGIN = 0.95;
    double expectedPower = current * voltage;

    auto rangeOpt = findPowerRange6310A(subModel, current / MARGIN);
    if (!rangeOpt) {
        qWarning() << "[Chroma6310ASpec] Unknown subModel:" << subModel << "- fallback CCH";
        return "CCH";
    }

    const PowerRangeSpec& range = *rangeOpt;

    bool currentOK = (current <= range.currentSpec.maxCurrent * MARGIN);
    bool powerOK   = (expectedPower <= range.power * MARGIN);
    bool voltageOK = (voltage >= range.voltageSpec.minVoltage &&
                      voltage <= range.voltageSpec.maxVoltage);

    if (!currentOK || !powerOK || !voltageOK) {
        qWarning() << "[Chroma6310ASpec] Range" << range.mode
                   << "found but constraints not met:"
                   << "currentOK=" << currentOK
                   << "powerOK="   << powerOK
                   << "voltageOK=" << voltageOK
                   << "- fallback CCH";
        return "CCH";
    }

    qDebug() << "[Chroma6310ASpec] Selected mode:" << range.mode
             << "SubModel:" << subModel
             << "I=" << current << "A"
             << "V=" << voltage << "V"
             << "P=" << expectedPower << "W";

    return range.mode;
}

// ==================== 延續型號（規格與 6310 相同，僅 model 字串帶 A）====================

// ──────────────────────────────────────────────────────────────────
//  63101A  80V  20W(CCL 0~4A) / 200W(CCH 0~40A)
// ──────────────────────────────────────────────────────────────────
ChromaLoadSpec createChroma63101ASpec()
{
    ChromaLoadSpec spec;
    spec.model = "63101A";

    PowerRangeSpec rangeCCL;
    rangeCCL.mode  = "CCL";
    rangeCCL.power = 20.0;
    rangeCCL.currentSpec = {
        0.0, 4.0, 0.001,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCL 低檔"
    };
    rangeCCL.voltageSpec = {
        0.8, 80.0, 0.02,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "0.8~80V"
    };
    rangeCCL.resistanceSpec = {
        {
            {0.0375, 150.0,   {0.1, 0.2, 0.0, "V", "150Ω : 0.1V+0.2%"}},
            {1.875,  7500.0,  {0.01, 0.1, 0.0, "V", "7.5KΩ : 0.01V+0.1%"}}
        }, 12, ""
    };
    rangeCCL.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.00064, 0.16, 0.00064, 0.000001,
        0.0, 4.0, 0.001, 0.004,
        "Slew:0.64~160mA/us; T1&T2:0.025ms~50ms"
    };
    rangeCCL.remark = "20W/CCL";

    PowerRangeSpec rangeCCH;
    rangeCCH.mode  = "CCH";
    rangeCCH.power = 200.0;
    rangeCCH.currentSpec = {
        0.0, 40.0, 0.01,
        {0.1, 0.2, 0.0, "%F.S.", "0.1%+0.2%F.S."},
        "CCH 高檔"
    };
    rangeCCH.voltageSpec    = rangeCCL.voltageSpec;
    rangeCCH.resistanceSpec = rangeCCL.resistanceSpec;
    rangeCCH.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.0064, 1.6, 0.0064, 0.000001,
        0.0, 40.0, 0.01, 0.004,
        "Slew:6.4~1600mA/us; T1&T2:0.025ms~50ms"
    };
    rangeCCH.remark = "200W/CCH";

    spec.ranges = { rangeCCL, rangeCCH };
    return spec;
}

// ──────────────────────────────────────────────────────────────────
//  63102A  80V  20W×2(CCL 0~2A) / 100W×2(CCH 0~20A)  雙通道
// ──────────────────────────────────────────────────────────────────
ChromaLoadSpec createChroma63102ASpec()
{
    ChromaLoadSpec spec;
    spec.model = "63102A";

    PowerRangeSpec rangeCCL;
    rangeCCL.mode  = "CCL";
    rangeCCL.power = 20.0;
    rangeCCL.currentSpec = {
        0.0, 2.0, 0.0005,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCL 低檔（單通道 20W）"
    };
    rangeCCL.voltageSpec = {
        0.8, 80.0, 0.02,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "0.8~80V"
    };
    rangeCCL.resistanceSpec = {
        {
            {0.075,  300.0,   {0.1, 0.2, 0.0, "V", "300Ω : 0.1V+0.2%"}},
            {3.75,  15000.0,  {0.01, 0.1, 0.0, "V", "15KΩ : 0.01V+0.1%"}}
        }, 12, ""
    };
    rangeCCL.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.00032, 0.08, 0.00032, 0.000001,
        0.0, 2.0, 0.0005, 0.004,
        "Slew:0.32~80mA/us"
    };
    rangeCCL.remark = "20W/CCL";

    PowerRangeSpec rangeCCH;
    rangeCCH.mode  = "CCH";
    rangeCCH.power = 100.0;
    rangeCCH.currentSpec = {
        0.0, 20.0, 0.005,
        {0.1, 0.2, 0.0, "%F.S.", "0.1%+0.2%F.S."},
        "CCH 高檔（單通道 100W）"
    };
    rangeCCH.voltageSpec    = rangeCCL.voltageSpec;
    rangeCCH.resistanceSpec = rangeCCL.resistanceSpec;
    rangeCCH.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.0032, 0.8, 0.0032, 0.000001,
        0.0, 20.0, 0.005, 0.004,
        "Slew:3.2~800mA/us"
    };
    rangeCCH.remark = "100W/CCH";

    spec.ranges = { rangeCCL, rangeCCH };
    return spec;
}

// ──────────────────────────────────────────────────────────────────
//  63103A  80V  30W(CCL 0~6A) / 300W(CCH 0~60A)
// ──────────────────────────────────────────────────────────────────
ChromaLoadSpec createChroma63103ASpec()
{
    ChromaLoadSpec spec;
    spec.model = "63103A";

    PowerRangeSpec rangeCCL;
    rangeCCL.mode  = "CCL";
    rangeCCL.power = 30.0;
    rangeCCL.currentSpec = {
        0.0, 6.0, 0.0015,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCL 低檔"
    };
    rangeCCL.voltageSpec = {
        0.8, 80.0, 0.02,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "0.8~80V"
    };
    rangeCCL.resistanceSpec = {
        {
            {0.025, 100.0,   {0.1, 0.2, 0.0, "V", "100Ω : 0.1V+0.2%"}},
            {1.25,  5000.0,  {0.01, 0.1, 0.0, "V", "5KΩ : 0.01V+0.1%"}}
        }, 12, ""
    };
    rangeCCL.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.001, 0.25, 0.001, 0.000001,
        0.0, 6.0, 0.0015, 0.004,
        "Slew:1~250mA/us"
    };
    rangeCCL.remark = "30W/CCL";

    PowerRangeSpec rangeCCH;
    rangeCCH.mode  = "CCH";
    rangeCCH.power = 300.0;
    rangeCCH.currentSpec = {
        0.0, 60.0, 0.015,
        {0.1, 0.2, 0.0, "%F.S.", "0.1%+0.2%F.S."},
        "CCH 高檔"
    };
    rangeCCH.voltageSpec    = rangeCCL.voltageSpec;
    rangeCCH.resistanceSpec = rangeCCL.resistanceSpec;
    rangeCCH.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.01, 2.5, 0.01, 0.000001,
        0.0, 60.0, 0.015, 0.004,
        "Slew:10~2500mA/us"
    };
    rangeCCH.remark = "300W/CCH";

    spec.ranges = { rangeCCL, rangeCCH };
    return spec;
}

// ──────────────────────────────────────────────────────────────────
//  63105A  500V  30W(CCL 0~1A) / 300W(CCH 0~10A)
// ──────────────────────────────────────────────────────────────────
ChromaLoadSpec createChroma63105ASpec()
{
    ChromaLoadSpec spec;
    spec.model = "63105A";

    PowerRangeSpec rangeCCL;
    rangeCCL.mode  = "CCL";
    rangeCCL.power = 30.0;
    rangeCCL.currentSpec = {
        0.0, 1.0, 0.00025,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCL 低檔"
    };
    rangeCCL.voltageSpec = {
        2.0, 500.0, 0.125,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "2~500V"
    };
    rangeCCL.resistanceSpec = {
        {
            {1.25,  5000.0,  {0.02, 0.2, 0.0, "V", "5KΩ : 20mV+0.2%"}},
            {50.0, 200000.0, {0.005, 0.1, 0.0, "V", "200KΩ : 5mV+0.1%"}}
        }, 12, ""
    };
    rangeCCL.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.00016, 0.04, 0.00016, 0.000001,
        0.0, 1.0, 0.00025, 0.004,
        "Slew:0.16~40mA/us"
    };
    rangeCCL.remark = "30W/CCL";

    PowerRangeSpec rangeCCH;
    rangeCCH.mode  = "CCH";
    rangeCCH.power = 300.0;
    rangeCCH.currentSpec = {
        0.0, 10.0, 0.0025,
        {0.1, 0.2, 0.0, "%F.S.", "0.1%+0.2%F.S."},
        "CCH 高檔"
    };
    rangeCCH.voltageSpec    = rangeCCL.voltageSpec;
    rangeCCH.resistanceSpec = rangeCCL.resistanceSpec;
    rangeCCH.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.0016, 0.4, 0.0016, 0.000001,
        0.0, 10.0, 0.0025, 0.004,
        "Slew:1.6~400mA/us"
    };
    rangeCCH.remark = "300W/CCH";

    spec.ranges = { rangeCCL, rangeCCH };
    return spec;
}

// ──────────────────────────────────────────────────────────────────
//  63106A  80V  60W(CCL 0~12A) / 600W(CCH 0~120A)
// ──────────────────────────────────────────────────────────────────
ChromaLoadSpec createChroma63106ASpec()
{
    ChromaLoadSpec spec;
    spec.model = "63106A";

    PowerRangeSpec rangeCCL;
    rangeCCL.mode  = "CCL";
    rangeCCL.power = 60.0;
    rangeCCL.currentSpec = {
        0.0, 12.0, 0.003,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCL 低檔"
    };
    rangeCCL.voltageSpec = {
        0.8, 80.0, 0.02,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "0.8~80V"
    };
    rangeCCL.resistanceSpec = {
        {
            {0.0125, 50.0,    {0.1, 0.5, 0.0, "V", "50Ω : 0.1V+0.5%"}},
            {0.625,  2500.0,  {0.01, 0.2, 0.0, "V", "2.5KΩ : 0.01V+0.2%"}}
        }, 12, ""
    };
    rangeCCL.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.002, 0.5, 0.002, 0.000001,
        0.0, 12.0, 0.003, 0.004,
        "Slew:2~500mA/us"
    };
    rangeCCL.remark = "60W/CCL";

    PowerRangeSpec rangeCCH;
    rangeCCH.mode  = "CCH";
    rangeCCH.power = 600.0;
    rangeCCH.currentSpec = {
        0.0, 120.0, 0.03,
        {0.1, 0.2, 0.0, "%F.S.", "0.1%+0.2%F.S."},
        "CCH 高檔"
    };
    rangeCCH.voltageSpec    = rangeCCL.voltageSpec;
    rangeCCH.resistanceSpec = rangeCCL.resistanceSpec;
    rangeCCH.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.02, 5.0, 0.02, 0.000001,
        0.0, 120.0, 0.03, 0.004,
        "Slew:20~5000mA/us"
    };
    rangeCCH.remark = "600W/CCH";

    spec.ranges = { rangeCCL, rangeCCH };
    return spec;
}

// ──────────────────────────────────────────────────────────────────
//  63107A  80V  雙通道（左:30W/5A；右通道:CCL 30W/4A + CCH 250W/40A）
//  本驅動以右通道（CCL/CCH）規格為準
//  ⚠ 左通道固定 30W/5A，需在上層以 CHAN 選擇
// ──────────────────────────────────────────────────────────────────
ChromaLoadSpec createChroma63107ASpec()
{
    ChromaLoadSpec spec;
    spec.model = "63107A";

    // 右通道 CCL（30W, 0~4A）
    PowerRangeSpec rangeCCL;
    rangeCCL.mode  = "CCL";
    rangeCCL.power = 30.0;
    rangeCCL.currentSpec = {
        0.0, 4.0, 0.001,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "右通道 CCL 低檔"
    };
    rangeCCL.voltageSpec = {
        0.8, 80.0, 0.02,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "0.8~80V"
    };
    rangeCCL.resistanceSpec = {
        {
            {0.0375, 150.0,  {0.1, 0.2, 0.0, "V", "150Ω : 0.1V+0.2%"}},
            {1.875,  7500.0, {0.01, 0.1, 0.0, "V", "7.5KΩ : 0.01V+0.1%"}}
        }, 12, ""
    };
    rangeCCL.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.00064, 0.16, 0.00064, 0.000001,
        0.0, 4.0, 0.001, 0.004,
        "Slew:0.64~160mA/us"
    };
    rangeCCL.remark = "30W/CCL（右通道低檔）";

    // 右通道 CCH（250W, 0~40A）
    PowerRangeSpec rangeCCH;
    rangeCCH.mode  = "CCH";
    rangeCCH.power = 250.0;
    rangeCCH.currentSpec = {
        0.0, 40.0, 0.01,
        {0.1, 0.2, 0.0, "%F.S.", "0.1%+0.2%F.S."},
        "右通道 CCH 高檔"
    };
    rangeCCH.voltageSpec    = rangeCCL.voltageSpec;
    rangeCCH.resistanceSpec = rangeCCL.resistanceSpec;
    rangeCCH.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.0064, 1.6, 0.0064, 0.000001,
        0.0, 40.0, 0.01, 0.004,
        "Slew:6.4~1600mA/us"
    };
    rangeCCH.remark = "250W/CCH（右通道高檔）";

    spec.ranges = { rangeCCL, rangeCCH };
    return spec;
}

// ──────────────────────────────────────────────────────────────────
//  63108A  500V  60W(CCL 0~2A) / 600W(CCH 0~20A)
// ──────────────────────────────────────────────────────────────────
ChromaLoadSpec createChroma63108ASpec()
{
    ChromaLoadSpec spec;
    spec.model = "63108A";

    PowerRangeSpec rangeCCL;
    rangeCCL.mode  = "CCL";
    rangeCCL.power = 60.0;
    rangeCCL.currentSpec = {
        0.0, 2.0, 0.0005,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCL 低檔"
    };
    rangeCCL.voltageSpec = {
        2.0, 500.0, 0.125,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "2~500V"
    };
    rangeCCL.resistanceSpec = {
        {
            {0.625,  2500.0,  {0.05, 0.2, 0.0, "V", "2.5KΩ : 50mV+0.2%"}},
            {25.0, 100000.0,  {0.005, 0.1, 0.0, "V", "100KΩ : 5mV+0.1%"}}
        }, 12, ""
    };
    rangeCCL.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.00032, 0.08, 0.00032, 0.000001,
        0.0, 2.0, 0.0005, 0.004,
        "Slew:0.32~80mA/us"
    };
    rangeCCL.remark = "60W/CCL";

    PowerRangeSpec rangeCCH;
    rangeCCH.mode  = "CCH";
    rangeCCH.power = 600.0;
    rangeCCH.currentSpec = {
        0.0, 20.0, 0.005,
        {0.1, 0.2, 0.0, "%F.S.", "0.1%+0.2%F.S."},
        "CCH 高檔"
    };
    rangeCCH.voltageSpec    = rangeCCL.voltageSpec;
    rangeCCH.resistanceSpec = rangeCCL.resistanceSpec;
    rangeCCH.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.0032, 0.8, 0.0032, 0.000001,
        0.0, 20.0, 0.005, 0.004,
        "Slew:3.2~800mA/us"
    };
    rangeCCH.remark = "600W/CCH";

    spec.ranges = { rangeCCL, rangeCCH };
    return spec;
}

// ──────────────────────────────────────────────────────────────────
//  63110A  LED 模擬負載  500V  100W×2
//  ⚠ 以 CC 電流範圍紀錄；LED 模式（LEDL/LEDH）由驅動層的 setLedMode() 控制
//  ⚠ 無 PROGRAM 子系統
//
//  Mode 選擇邏輯（對照規格）：
//    CCL  100V電壓檔  0~0.6A（低電流 LED）
//    CCH  500V電壓檔  0~2A  （高電流 LED）
// ──────────────────────────────────────────────────────────────────
ChromaLoadSpec createChroma63110ASpec()
{
    ChromaLoadSpec spec;
    spec.model = "63110A";

    // CCL：低電流範圍 0~0.6A，100V 電壓檔
    PowerRangeSpec rangeCCL;
    rangeCCL.mode  = "CCL";
    rangeCCL.power = 100.0;
    rangeCCL.currentSpec = {
        0.0, 0.6, 0.000012,   // 解析度 12µA
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCL / LEDL（低電流，100V 檔）"
    };
    rangeCCL.voltageSpec = {
        0.9, 100.0, 0.002,
        {0.025, 0.025, 0.0, "%F.S.", "0.025%+0.025%F.S."},
        "0.9~100V"
    };
    rangeCCL.resistanceSpec = {
        {
         {3.0, 1000.0, {0.004, 0.2, 0.0, "V", "CRL: 0.004S+0.2%"}},
         }, 0, "CRL 3Ω~1kΩ"
    };
    // 63110A 無動態模擬時序規格
    rangeCCL.dynamicSpec = {};
    rangeCCL.remark = "100W/CCL（LEDL 低電流）";

    // CCH：高電流範圍 0~2A，500V 電壓檔
    PowerRangeSpec rangeCCH;
    rangeCCH.mode  = "CCH";
    rangeCCH.power = 100.0;
    rangeCCH.currentSpec = {
        0.0, 2.0, 0.00004,    // 解析度 40µA
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCH / LEDH（高電流，500V 檔）"
    };
    rangeCCH.voltageSpec = {
        6.0, 500.0, 0.02,
        {0.025, 0.025, 0.0, "%F.S.", "0.025%+0.025%F.S."},
        "6~500V"
    };
    rangeCCH.resistanceSpec = {
        {
         {10.0, 10000.0, {0.001, 0.1, 0.0, "V", "CRH: 0.001S+0.1%"}},
         }, 0, "CRH 10Ω~10kΩ"
    };
    rangeCCH.dynamicSpec = {};
    rangeCCH.remark = "100W/CCH（LEDH 高電流）";

    spec.ranges = { rangeCCL, rangeCCH };
    return spec;
}

// ──────────────────────────────────────────────────────────────────
//  63112A  80V  120W(CCL 0~24A) / 1200W(CCH 0~240A)
// ──────────────────────────────────────────────────────────────────
ChromaLoadSpec createChroma63112ASpec()
{
    ChromaLoadSpec spec;
    spec.model = "63112A";

    PowerRangeSpec rangeCCL;
    rangeCCL.mode  = "CCL";
    rangeCCL.power = 120.0;
    rangeCCL.currentSpec = {
        0.0, 24.0, 0.006,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCL 低檔"
    };
    rangeCCL.voltageSpec = {
        0.8, 80.0, 0.02,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "0.8~80V"
    };
    rangeCCL.resistanceSpec = {
        {
            {6.25e-3, 25.0,   {0.1, 0.8, 0.0, "V", "25Ω : 0.1V+0.8%"}},
            {0.3125, 1250.0,  {0.01, 0.2, 0.0, "V", "1.25KΩ : 0.01V+0.2%"}}
        }, 12, ""
    };
    rangeCCL.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.004, 1.0, 0.004, 0.000001,
        0.0, 24.0, 0.006, 0.004,
        "Slew:4~1000mA/us"
    };
    rangeCCL.remark = "120W/CCL";

    PowerRangeSpec rangeCCH;
    rangeCCH.mode  = "CCH";
    rangeCCH.power = 1200.0;
    rangeCCH.currentSpec = {
        0.0, 240.0, 0.06,
        {0.1, 0.2, 0.0, "%F.S.", "0.1%+0.2%F.S."},
        "CCH 高檔"
    };
    rangeCCH.voltageSpec    = rangeCCL.voltageSpec;
    rangeCCH.resistanceSpec = rangeCCL.resistanceSpec;
    rangeCCH.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.04, 10.0, 0.04, 0.000001,
        0.0, 240.0, 0.06, 0.004,
        "Slew:40~10000mA/us"
    };
    rangeCCH.remark = "1200W/CCH";

    spec.ranges = { rangeCCL, rangeCCH };
    return spec;
}

// ==================== 新增型號 ====================

// ──────────────────────────────────────────────────────────────────
//  63113A  300V  300W (CCL 0~5A / CCH 0~20A)
//  具備 LED 模式（LEDL/LEDH）
// ──────────────────────────────────────────────────────────────────
ChromaLoadSpec createChroma63113ASpec()
{
    ChromaLoadSpec spec;
    spec.model = "63113A";

    PowerRangeSpec rangeCCL;
    rangeCCL.mode  = "CCL";
    rangeCCL.power = 300.0;
    rangeCCL.currentSpec = {
        0.0, 5.0, 0.0001,      // 解析度 100µA
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCL 低檔"
    };
    rangeCCL.voltageSpec = {
        1.0, 300.0, 0.006,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "1~300V"
    };
    rangeCCL.resistanceSpec = {
        {
            {0.2,  200.0,  {0.01, 0.2, 0.0, "V", "CRL@CH: 10mS+0.2%"}},
            {0.8,  800.0,  {0.0025, 0.2, 0.0, "V", "CRL@CL: 2.5mS+0.2%"}}
        }, 0, "CRL@CH 0.2~200Ω / CRL@CL 0.8~800Ω"
    };
    rangeCCL.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.0008, 0.2, 0.0008, 0.000001,
        0.0, 5.0, 0.0001, 0.004,
        "Slew:0.8~200mA/us (CCL)"
    };
    rangeCCL.remark = "300W/CCL";

    PowerRangeSpec rangeCCH;
    rangeCCH.mode  = "CCH";
    rangeCCH.power = 300.0;
    rangeCCH.currentSpec = {
        0.0, 20.0, 0.0004,     // 解析度 400µA
        {0.1, 0.2, 0.0, "%F.S.", "0.1%+0.2%F.S."},
        "CCH 高檔"
    };
    rangeCCH.voltageSpec = {
        4.0, 300.0, 0.006,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "4~300V"
    };
    rangeCCH.resistanceSpec = {
        {
            {4.0, 4000.0, {0.0005, 0.2, 0.0, "V", "CRH@CL: 0.5mS+0.2%"}}
        }, 0, "CRH@CL 4~4kΩ"
    };
    rangeCCH.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.0032, 0.8, 0.0032, 0.000001,
        0.0, 20.0, 0.0004, 0.004,
        "Slew:3.2~800mA/us (CCH)"
    };
    rangeCCH.remark = "300W/CCH";

    spec.ranges = { rangeCCL, rangeCCH };
    return spec;
}

// ──────────────────────────────────────────────────────────────────
//  63115A  600V  300W (CCL 0~5A / CCH 0~20A)
//  具備 LED 模式（LEDL/LEDH）
// ──────────────────────────────────────────────────────────────────
ChromaLoadSpec createChroma63115ASpec()
{
    ChromaLoadSpec spec;
    spec.model = "63115A";

    PowerRangeSpec rangeCCL;
    rangeCCL.mode  = "CCL";
    rangeCCL.power = 300.0;
    rangeCCL.currentSpec = {
        0.0, 5.0, 0.0001,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCL 低檔"
    };
    rangeCCL.voltageSpec = {
        1.0, 600.0, 0.012,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "1~600V"
    };
    rangeCCL.resistanceSpec = {
        {
            {0.2,  200.0,  {0.01, 0.2, 0.0, "V", "CRL@CH: 10mS+0.2%"}},
            {0.8,  800.0,  {0.0025, 0.2, 0.0, "V", "CRL@CL: 2.5mS+0.2%"}}
        }, 0, "CRL@CH 0.2~200Ω / CRL@CL 0.8~800Ω"
    };
    rangeCCL.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.0008, 0.2, 0.0008, 0.000001,
        0.0, 5.0, 0.0001, 0.004,
        "Slew:0.8~200mA/us (CCL)"
    };
    rangeCCL.remark = "300W/CCL";

    PowerRangeSpec rangeCCH;
    rangeCCH.mode  = "CCH";
    rangeCCH.power = 300.0;
    rangeCCH.currentSpec = {
        0.0, 20.0, 0.0004,
        {0.1, 0.2, 0.0, "%F.S.", "0.1%+0.2%F.S."},
        "CCH 高檔"
    };
    rangeCCH.voltageSpec = {
        4.0, 600.0, 0.012,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "4~600V"
    };
    rangeCCH.resistanceSpec = {
        {
            {8.0, 8000.0, {0.00025, 0.2, 0.0, "V", "CRH@CL: 0.25mS+0.2%"}}
        }, 0, "CRH@CL 8~8kΩ"
    };
    rangeCCH.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.0032, 0.8, 0.0032, 0.000001,
        0.0, 20.0, 0.0004, 0.004,
        "Slew:3.2~800mA/us (CCH)"
    };
    rangeCCH.remark = "300W/CCH";

    spec.ranges = { rangeCCL, rangeCCH };
    return spec;
}

// ──────────────────────────────────────────────────────────────────
//  63123A  120V  350W  高精度（0.04%+0.04%F.S.）
//  低電壓高電流設計（最小操作電壓 0.6V@70A）
// ──────────────────────────────────────────────────────────────────
ChromaLoadSpec createChroma63123ASpec()
{
    ChromaLoadSpec spec;
    spec.model = "63123A";

    PowerRangeSpec rangeCCL;
    rangeCCL.mode  = "CCL";
    rangeCCL.power = 350.0;
    rangeCCL.currentSpec = {
        0.0, 7.0, 0.000125,   // 解析度 0.125mA
        {0.04, 0.04, 0.0, "%F.S.", "0.04%+0.04%F.S.（高精度）"},
        "CCL 低檔"
    };
    rangeCCL.voltageSpec = {
        0.1, 120.0, 0.002,
        {0.025, 0.015, 0.0, "%F.S.", "0.025%+0.015%F.S."},
        "0.1~120V"
    };
    rangeCCL.resistanceSpec = {
        {
            {0.015, 15.0,   {0.1, 0.667, 0.07, "A/Vin", "CRL@CH: 0.1%+0.667S"}},
            {0.15,  150.0,  {0.1, 66.7e-3, 0.007, "A/Vin", "CRL@CL: 0.1%+66.7mS"}},
            {2.0,   2000.0, {0.2, 5e-3, 0.07, "A/Vin", "CRH@CH: 0.2%+5mS"}},
            {11.5, 11500.0, {0.2, 0.87e-3, 0.007, "A/Vin", "CRH@CL: 0.2%+0.87mS"}}
        }, 0, "CRL@CH/CL / CRH@CH/CL"
    };
    rangeCCL.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.0001, 0.025, 0.0001, 0.000001,
        0.0, 7.0, 0.000125, 0.001,
        "Slew:0.1~25mA/us (低檔 L1)"
    };
    rangeCCL.remark = "350W/CCL（高精度）";

    PowerRangeSpec rangeCCH;
    rangeCCH.mode  = "CCH";
    rangeCCH.power = 350.0;
    rangeCCH.currentSpec = {
        0.0, 70.0, 0.00125,   // 解析度 1.25mA
        {0.04, 0.04, 0.0, "%F.S.", "0.04%+0.04%F.S.（高精度）"},
        "CCH 高檔"
    };
    rangeCCH.voltageSpec = {
        0.6, 120.0, 0.002,
        {0.025, 0.015, 0.0, "%F.S.", "0.025%+0.015%F.S."},
        "0.6~120V"
    };
    rangeCCH.resistanceSpec = rangeCCL.resistanceSpec;
    rangeCCH.dynamicSpec = {
        0.000025, 0.05, 0.000005,
        0.001, 0.25, 0.001, 0.000001,
        0.0, 70.0, 0.00125, 0.001,
        "Slew:1~250mA/us (高檔 L1); 10m~2.5A/us (高檔 L2)"
    };
    rangeCCH.remark = "350W/CCH（高精度）";

    spec.ranges = { rangeCCL, rangeCCH };
    return spec;
}
