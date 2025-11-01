#include "chromaload6310spec.h"
#include <QMap>
#include <QString>
#include <functional>
#include <optional>
#include <QDebug>


QMap<QString, std::function<ChromaLoadSpec()>> createSpecMap = {
    { "63101", createChroma63101Spec },
    { "63102", createChroma63102Spec },
    { "63103", createChroma63103Spec },
    { "63105", createChroma63105Spec },
    { "63106", createChroma63106Spec },
    { "63108", createChroma63108Spec },
    { "63112", createChroma63112Spec },
    };


// 回傳 PowerRangeSpec，精確描述是哪一檔（低檔or高檔）
std::optional<PowerRangeSpec> findPowerRange(const QString& subModel, double currval)
{
    if (!createSpecMap.contains(subModel)) return std::nullopt;
    ChromaLoadSpec spec = createSpecMap[subModel]();
    for (const auto& range : spec.ranges) {                 //ex:63103 spec.ranges = { range30, range300 };
        if (currval <= range.currentSpec.maxCurrent) {
            return range;
        }
    }
    // 沒找到則回傳空
    return std::nullopt;
}

ChromaLoadSpec createChroma63101Spec()
{
    ChromaLoadSpec spec;
    spec.model = "63101";

    // 20W Range
    PowerRangeSpec range20;
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
        0.000025, 0.01, 0.000001, // T1/T2: 0.025ms~10ms, Res: 1us
        0.00064, 0.16, 0.00064,   // Slew Rate: 0.64~160mA/us, Res: 0.64mA/us
        0.000001,                 // Accuracy: 1us (T1/T2, 實際還需 remark 標註 "1us/1ms+100ppm")
        0.0, 4.0, 0.001, 0.004,   // Current: 0~4A, Res: 1mA, CurrAcc: 0.4%F.S.
        "T1&T2: 0.025ms~10ms Res:1us; Slew:0.64~160mA/us Res:0.64mA/us; Current:0~4A Res:1mA; Accuracy:0.4%F.S."
    };
    range20.remark = "20W/CCL";

    // 200W Range
    PowerRangeSpec range200;
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
        0.000025, 0.01, 0.000001, // T1/T2: 0.025ms~10ms, Res: 1us
        0.0064, 1.6, 0.0064,      // Slew Rate: 6.4~1600mA/us, Res: 6.4mA/us
        0.000001,                 // Accuracy: 1us/1ms+100ppm（可寫到 remark）
        0.0, 40.0, 0.01, 0.004,   // Current: 0~40A, Res: 10mA, CurrAcc: 0.4%F.S.
        "T1&T2: 0.025ms~10ms Res:1us; Slew:6.4~1600mA/us Res:6.4mA/us; Current:0~40A Res:10mA; Accuracy:0.4%F.S."
    };
    range200.remark = "200W/CCH";

    // 加入所有 range
    spec.ranges = { range20, range200 };
    return spec;
}

ChromaLoadSpec createChroma63102Spec()
{
    ChromaLoadSpec spec;
    spec.model = "63102";

    // 100W*2, 兩通道，各 0~2A
    PowerRangeSpec range20;
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
        0.000025, 0.01, 0.000001, // T1/T2: 0.025ms~10ms, Res: 1us
        0.00032, 0.08, 0.00032,   // Slew Rate: 0.32~80mA/μs, Res: 0.32mA/μs
        0.000001,                 // Accuracy: 1us/1ms+100ppm
        0.0, 2.0, 0.0005, 0.004,  // Current: 0~2A, Res: 0.5mA, CurrAcc: 0.4%F.S.
        "T1&T2: 0.025ms~10ms Res:1us; Slew:0.32~80mA/μs Res:0.32mA/μs; Current:0~2A Res:0.5mA; Accuracy:0.4%F.S."
    };
    range20.remark = "20W/CCL";

    PowerRangeSpec range100;
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
        0.000025, 0.01, 0.000001, // T1/T2: 0.025ms~10ms, Res: 1us
        0.0032, 0.8, 0.0032,      // Slew Rate: 3.2~800mA/μs, Res: 3.2mA/μs
        0.000001,                 // Accuracy: 1us/1ms+100ppm
        0.0, 20.0, 0.005, 0.004,  // Current: 0~20A, Res: 5mA, CurrAcc: 0.4%F.S.
        "T1&T2: 0.025ms~10ms Res:1us; Slew:3.2~800mA/μs Res:3.2mA/μs; Current:0~20A Res:5mA; Accuracy:0.4%F.S."
    };
    range100.remark = "100W/CCH";

    spec.ranges = { range20, range100 };
    return spec;
}

ChromaLoadSpec createChroma63103Spec()
{
    ChromaLoadSpec spec;
    spec.model = "63103";

    // 30W Range (CCL)
    PowerRangeSpec range30;
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
        0.000025, 0.01, 0.000001, // T1/T2: 0.025ms~10ms, Res: 1us
        0.001, 0.25, 0.001,       // Slew Rate: 1~250mA/μs, Res: 1μA/μs
        0.000001,                 // Accuracy: 1us/1ms+100ppm
        0.0, 6.0, 0.0015, 0.004,  // Current: 0~6A, Res: 1.5mA, CurrAcc: 0.4%F.S.
        "T1&T2: 0.025ms~10ms Res:1us; Slew:1~250mA/μs Res:1μA/μs; Current:0~6A Res:1.5mA; Accuracy:0.4%F.S."
    };
    range30.remark = "30W/CCL";

    // 300W Range (CCH)
    PowerRangeSpec range300;
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
        0.000025, 0.01, 0.000001, // T1/T2: 0.025ms~10ms, Res: 1us
        0.01, 2.5, 0.01,          // Slew Rate: 10~2500mA/μs, Res: 10μA/μs
        0.000001,                 // Accuracy: 1us/1ms+100ppm
        0.0, 60.0, 0.015, 0.004,  // Current: 0~60A, Res: 15mA, CurrAcc: 0.4%F.S.
        "T1&T2: 0.025ms~10ms Res:1us; Slew:10~2500mA/μs Res:10μA/μs; Current:0~60A Res:15mA; Accuracy:0.4%F.S."
    };
    range300.remark = "300W/CCH";

    spec.ranges = { range30, range300 };
    return spec;
}


ChromaLoadSpec createChroma63105Spec()
{
    ChromaLoadSpec spec;
    spec.model = "63105";

    // 30W Range (低檔)
    PowerRangeSpec range30;
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
        0.000025, 0.01, 0.000001, // T1/T2: 0.025ms~10ms, Res: 1us
        0.00016, 0.04, 0.00016,   // Slew Rate: 0.16~40mA/μs, Res: 0.16mA/μs
        0.000001,                 // Accuracy: 1us/1ms+100ppm
        0.0, 1.0, 0.00025, 0.004, // Current: 0~1A, Res: 0.25mA, CurrAcc: 0.4%F.S.
        "T1&T2: 0.025ms~10ms Res:1us; Slew:0.16~40mA/μs Res:0.16mA/μs; Current:0~1A Res:0.25mA; Accuracy:0.4%F.S."
    };
    range30.remark = "30W/CCL";

    // 300W Range (高檔)
    PowerRangeSpec range300;
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
        0.000025, 0.01, 0.000001, // T1/T2: 0.025ms~10ms, Res: 1us
        0.0016, 0.4, 0.0016,      // Slew Rate: 1.6~400mA/μs, Res: 1.6mA/μs
        0.000001,                 // Accuracy: 1us/1ms+100ppm
        0.0, 10.0, 0.0025, 0.004, // Current: 0~10A, Res: 2.5mA, CurrAcc: 0.4%F.S.
        "T1&T2: 0.025ms~10ms Res:1us; Slew:1.6~400mA/μs Res:1.6mA/μs; Current:0~10A Res:2.5mA; Accuracy:0.4%F.S."
    };
    range300.remark = "300W/CCH";

    spec.ranges = { range30, range300 };
    return spec;
}

ChromaLoadSpec createChroma63106Spec()
{
    ChromaLoadSpec spec;
    spec.model = "63106";

    // 60W Range
    PowerRangeSpec range60;
    range60.power = 60.0;
    range60.currentSpec = {
        0.0, 12.0, 0.003,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "60W"
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
        0.000025, 0.01, 0.000001, // T1/T2: 0.025ms~10ms, Res: 1us
        0.002, 0.5, 0.002,        // Slew Rate: 0.002~0.5A/μs, Res: 0.002A/μs
        0.000001,                 // Accuracy: 1us/1ms+100ppm
        0.0, 12.0, 0.003, 0.004,  // Current: 0~12A, Res: 3mA, CurrAcc: 0.4%F.S.
        "T1&T2: 0.025ms~10ms Res:1us; Slew:2~500mA/μs Res:2mA/μs; Current:0~12A Res:3mA; Accuracy:0.4%F.S."
    };
    range60.remark = "60W/CCL";

    // 600W Range
    PowerRangeSpec range600;
    range600.power = 600.0;
    range600.currentSpec = {
        0.0, 120.0, 0.03,
        {0.1, 0.2, 0.0, "%F.S.", "0.1%+0.2%F.S."},
        "600W"
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
        0.000025, 0.01, 0.000001, // T1/T2: 0.025ms~10ms, Res: 1us
        0.02, 5.0, 0.02,          // Slew Rate: 0.02~5A/μs, Res: 0.02A/μs
        0.000001,                 // Accuracy: 1us/1ms+100ppm
        0.0, 120.0, 0.03, 0.004,  // Current: 0~120A, Res: 30mA, CurrAcc: 0.4%F.S.
        "T1&T2: 0.025ms~10ms Res:1us; Slew:20~5000mA/μs Res:20mA/μs; Current:0~120A Res:30mA; Accuracy:0.4%F.S."
    };
    range600.remark = "600W/CCH";

    spec.ranges = { range60, range600 };
    return spec;
}

ChromaLoadSpec createChroma63108Spec()
{
    ChromaLoadSpec spec;
    spec.model = "63108";

    // 60W Range
    PowerRangeSpec range60;
    range60.power = 60.0;
    range60.currentSpec = {
        0.0, 2.0, 0.0005,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "60W"
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
        0.000025, 0.01, 0.000001, // T1/T2: 0.025ms~10ms, Res: 1us
        0.00032, 0.08, 0.00032,   // Slew Rate: 0.32~80mA/μs, Res: 0.32mA/μs
        0.000001,                 // Accuracy: 1us/1ms+100ppm
        0.0, 2.0, 0.0005, 0.004,  // Current: 0~2A, Res: 0.5mA, CurrAcc: 0.4%F.S.
        "T1&T2: 0.025ms~10ms Res:1us; Slew:0.32~80mA/μs Res:0.32mA/μs; Current:0~2A Res:0.5mA; Accuracy:0.4%F.S."
    };
    range60.remark = "60W/CCL";

    // 600W Range
    PowerRangeSpec range600;
    range600.power = 600.0;
    range600.currentSpec = {
        0.0, 20.0, 0.005,
        {0.1, 0.2, 0.0, "%F.S.", "0.1%+0.2%F.S."},
        "600W"
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
        0.000025, 0.01, 0.000001, // T1/T2: 0.025ms~10ms, Res: 1us
        0.0032, 0.8, 0.0032,      // Slew Rate: 3.2~800mA/μs, Res: 3.2mA/μs
        0.000001,                 // Accuracy: 1us/1ms+100ppm
        0.0, 20.0, 0.005, 0.004,  // Current: 0~20A, Res: 5mA, CurrAcc: 0.4%F.S.
        "T1&T2: 0.025ms~10ms Res:1us; Slew:3.2~800mA/μs Res:3.2mA/μs; Current:0~20A Res:5mA; Accuracy:0.4%F.S."
    };
    range600.remark = "600W/CCH";

    spec.ranges = { range60, range600 };
    return spec;
}

ChromaLoadSpec createChroma63112Spec()
{
    ChromaLoadSpec spec;
    spec.model = "63112";

    // 120W Range
    PowerRangeSpec range120;
    range120.power = 120.0;
    range120.currentSpec = {
        0.0, 24.0, 0.006,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "120W"
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
        0.000025, 0.01, 0.000001, // T1/T2: 0.025ms~10ms, Res: 1us
        0.000004, 0.001, 0.000004, // Slew Rate: 0.004~1A/μs, Res: 0.004A/μs
        0.000001,                 // Accuracy: 1us/1ms+100ppm
        0.0, 24.0, 0.006, 0.004,  // Current: 0~24A, Res: 6mA, CurrAcc: 0.4%F.S.
        "T1&T2: 0.025ms~10ms Res:1us; Slew:4~1000mA/μs Res:4mA/μs; Current:0~24A Res:6mA; Accuracy:0.4%F.S."
    };
    range120.remark = "120W/CCL";

    // 1200W Range
    PowerRangeSpec range1200;
    range1200.power = 1200.0;
    range1200.currentSpec = {
        0.0, 240.0, 0.06,
        {0.1, 0.2, 0.0, "%F.S.", "0.1%+0.2%F.S."},
        "1200W"
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
        0.000025, 0.01, 0.000001, // T1/T2: 0.025ms~10ms, Res: 1us
        0.00004, 0.01, 0.00004,   // Slew Rate: 0.04~10A/μs, Res: 0.04A/μs
        0.000001,                 // Accuracy: 1us/1ms+100ppm
        0.0, 240.0, 0.06, 0.004,  // Current: 0~240A, Res: 60mA, CurrAcc: 0.4%F.S.
        "T1&T2: 0.025ms~10ms Res:1us; Slew:40~10000mA/μs Res:40mA/μs; Current:0~240A Res:60mA; Accuracy:0.4%F.S."
    };
    range1200.remark = "1200W/CCH";

    spec.ranges = { range120, range1200 };
    return spec;
}

QString selectOptimalLoadMode(const QString& subModel,
                              double current,
                              double voltage)
{
    double expectedPower = current * voltage;

    if (!createSpecMap.contains(subModel)) {
        qWarning() << "[Chroma6310Spec] Unknown subModel:" << subModel;
        return "CCL";  // fallback
    }

    ChromaLoadSpec spec = createSpecMap[subModel]();
    QString selectedMode = "CCL";
    bool foundSuitable = false;

    // 從低檔開始檢查,選擇能滿足功率和電流的最小檔位
    for (const auto& range : spec.ranges) {
        bool currentOK = (current <= range.currentSpec.maxCurrent * 0.95);  // 5% 餘裕
        bool powerOK = (expectedPower <= range.power * 0.95);              // 5% 餘裕
        bool voltageOK = (voltage >= range.voltageSpec.minVoltage &&
                          voltage <= range.voltageSpec.maxVoltage);

        if (currentOK && powerOK && voltageOK) {
            foundSuitable = true;
            // 根據 remark 判斷是 CCL 還是 CCH
            if (range.currentSpec.remark.contains("CCH")) {
                selectedMode = "CCH";
            } else if (range.currentSpec.remark.contains("CCL")) {
                selectedMode = "CCL";
            }

            qDebug() << "[Chroma6310Spec] Selected mode:" << selectedMode
                     << "for SubModel:" << subModel
                     << "I=" << current << "A, V=" << voltage << "V, P=" << expectedPower << "W";
            break;  // 選擇第一個合適的 (通常是較低檔)
        }
    }

    if (!foundSuitable) {
        qWarning() << "[Chroma6310Spec] ⚠️ WARNING: No suitable range found!";
        qWarning() << "  SubModel:" << subModel;
        qWarning() << "  Current:" << current << "A";
        qWarning() << "  Voltage:" << voltage << "V";
        qWarning() << "  Expected Power:" << expectedPower << "W";
        qWarning() << "  ⚠️ May trigger OPP protection!";
        qWarning() << "  Forcing CCH mode to minimize risk...";

        // 強制選擇高檔以降低風險
        selectedMode = "CCH";
    }

    return selectedMode;
}
