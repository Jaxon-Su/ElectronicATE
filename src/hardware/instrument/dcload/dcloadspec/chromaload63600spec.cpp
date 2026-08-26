#include "chromaload63600spec.h"
#include <QMap>
#include <QString>
#include <functional>
#include <optional>
#include <QDebug>

// ==================== 規格映射表 ====================

static QMap<QString, std::function<ChromaLoad63600Spec()>> createSpecMap63600 = {
    { "63610-80-20",  createChroma636108020Spec  },
    { "63630-80-60",  createChroma636308060Spec  },
    { "63640-80-80",  createChroma636408080Spec  },
    { "63640-150-60", createChroma6364015060Spec },
    { "63630-600-15", createChroma6363060015Spec },
    };

// ==================== findPowerRange63600 ====================

std::optional<PowerRangeSpec63600> findPowerRange63600(const QString& subModel, double currval)
{
    if (!createSpecMap63600.contains(subModel)) {
        qWarning() << "[Chroma63600Spec] Unknown subModel:" << subModel;
        return std::nullopt;
    }

    ChromaLoad63600Spec spec = createSpecMap63600[subModel]();

    for (const auto& range : spec.ranges) {
        if (currval <= range.currentSpec.maxCurrent) {
            return range;
        }
    }

    // 超出最大電流範圍 → 回傳最後一個範圍（CCH），讓硬體自行保護
    if (!spec.ranges.isEmpty()) {
        qWarning() << "[Chroma63600Spec] Current" << currval
                   << "A exceeds all ranges for" << subModel
                   << "- using CCH (last range)";
        return spec.ranges.last();
    }

    return std::nullopt;
}

// ==================== selectOptimalLoadMode63600 ====================

QString selectOptimalLoadMode63600(const QString& subModel,
                                   double current,
                                   double voltage)
{
    const double MARGIN = 0.95;
    double expectedPower = current * voltage;

    // Step 1：找到適合電流的範圍
    auto rangeOpt = findPowerRange63600(subModel, current / MARGIN);
    if (!rangeOpt) {
        qWarning() << "[Chroma63600Spec] Unknown subModel:" << subModel << "- fallback CCH";
        return "CCH";
    }

    const PowerRangeSpec63600& range = *rangeOpt;

    // Step 2：驗證功率與電壓是否在規格內（95% margin）
    bool currentOK = (current <= range.currentSpec.maxCurrent * MARGIN);
    bool powerOK   = (expectedPower <= range.power * MARGIN);
    bool voltageOK = (voltage >= range.voltageSpec.minVoltage &&
                      voltage <= range.voltageSpec.maxVoltage);

    if (!currentOK || !powerOK || !voltageOK) {
        qWarning() << "[Chroma63600Spec] Range" << range.mode
                   << "found but constraints not met:"
                   << "currentOK=" << currentOK
                   << "powerOK="   << powerOK
                   << "voltageOK=" << voltageOK
                   << "- fallback CCH";
        return "CCH";
    }

    // Step 3：直接回傳 mode 欄位（不依賴 index 推斷）
    qDebug() << "[Chroma63600Spec] Selected mode:" << range.mode
             << "SubModel:" << subModel
             << "I=" << current << "A"
             << "V=" << voltage << "V"
             << "P=" << expectedPower << "W";

    return range.mode;
}

// ==================== 63610-80-20 規格 (100Wx2, 0~80V, 0~20A) ====================

ChromaLoad63600Spec createChroma636108020Spec()
{
    ChromaLoad63600Spec spec;
    spec.model = "63610-80-20";

    // CCL 低檔 (0~0.2A)
    PowerRangeSpec63600 rangeLow;
    rangeLow.mode  = "CCL";
    rangeLow.power = 16.0;
    rangeLow.currentSpec = {
        0.0, 0.2, 0.00001,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCL"
    };
    rangeLow.voltageSpec = {
        0.5, 80.0, 0.0001,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "6V range"
    };
    rangeLow.dynamicSpec = {
        0.00001, 0.01, 0.000001,
        0.00001, 0.02, 0.00001,
        0.000001,
        0.0, 0.2, 0.00001, 0.004,
        "CCL Dynamic"
    };
    rangeLow.remark = "16W/CCL";

    // CCM 中檔 (0~2A)
    PowerRangeSpec63600 rangeMid;
    rangeMid.mode  = "CCM";
    rangeMid.power = 30.0;
    rangeMid.currentSpec = {
        0.0, 2.0, 0.0001,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCM"
    };
    rangeMid.voltageSpec = {
        0.5, 80.0, 0.001,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "16V range"
    };
    rangeMid.dynamicSpec = {
        0.00001, 0.01, 0.000001,
        0.0001, 0.2, 0.0001,
        0.000001,
        0.0, 2.0, 0.0001, 0.004,
        "CCM Dynamic"
    };
    rangeMid.remark = "30W/CCM";

    // CCH 高檔 (0~20A)
    PowerRangeSpec63600 rangeHigh;
    rangeHigh.mode  = "CCH";
    rangeHigh.power = 100.0;
    rangeHigh.currentSpec = {
        0.0, 20.0, 0.001,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCH"
    };
    rangeHigh.voltageSpec = {
        0.5, 80.0, 0.001,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "80V range"
    };
    rangeHigh.dynamicSpec = {
        0.00001, 0.01, 0.000001,
        0.001, 2.0, 0.001,
        0.000001,
        0.0, 20.0, 0.001, 0.004,
        "CCH Dynamic"
    };
    rangeHigh.remark = "100W/CCH";

    spec.ranges = { rangeLow, rangeMid, rangeHigh };
    return spec;
}

// ==================== 63630-80-60 規格 (300W, 0~80V, 0~60A) ====================

ChromaLoad63600Spec createChroma636308060Spec()
{
    ChromaLoad63600Spec spec;
    spec.model = "63630-80-60";

    // CCL 低檔 (0~0.6A)
    PowerRangeSpec63600 rangeLow;
    rangeLow.mode  = "CCL";
    rangeLow.power = 30.0;
    rangeLow.currentSpec = {
        0.0, 0.6, 0.00001,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCL"
    };
    rangeLow.voltageSpec = {
        0.5, 80.0, 0.0001,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "6V range"
    };
    rangeLow.dynamicSpec = {
        0.00001, 0.01, 0.000001,
        0.00001, 0.06, 0.00001,
        0.000001,
        0.0, 0.6, 0.00001, 0.004,
        "CCL Dynamic"
    };
    rangeLow.remark = "30W/CCL";

    // CCM 中檔 (0~6A)
    PowerRangeSpec63600 rangeMid;
    rangeMid.mode  = "CCM";
    rangeMid.power = 30.0;
    rangeMid.currentSpec = {
        0.0, 6.0, 0.0001,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCM"
    };
    rangeMid.voltageSpec = {
        0.5, 80.0, 0.001,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "16V range"
    };
    rangeMid.dynamicSpec = {
        0.00001, 0.01, 0.000001,
        0.0001, 0.6, 0.0001,
        0.000001,
        0.0, 6.0, 0.0001, 0.004,
        "CCM Dynamic"
    };
    rangeMid.remark = "30W/CCM";

    // CCH 高檔 (0~60A)
    PowerRangeSpec63600 rangeHigh;
    rangeHigh.mode  = "CCH";
    rangeHigh.power = 300.0;
    rangeHigh.currentSpec = {
        0.0, 60.0, 0.001,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCH"
    };
    rangeHigh.voltageSpec = {
        0.5, 80.0, 0.001,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "80V range"
    };
    rangeHigh.dynamicSpec = {
        0.00001, 0.01, 0.000001,
        0.001, 6.0, 0.001,
        0.000001,
        0.0, 60.0, 0.001, 0.004,
        "CCH Dynamic"
    };
    rangeHigh.remark = "300W/CCH";

    spec.ranges = { rangeLow, rangeMid, rangeHigh };
    return spec;
}

// ==================== 63640-80-80 規格 (400W, 0~80V, 0~80A) ====================

ChromaLoad63600Spec createChroma636408080Spec()
{
    ChromaLoad63600Spec spec;
    spec.model = "63640-80-80";

    // CCL 低檔 (0~0.8A)
    PowerRangeSpec63600 rangeLow;
    rangeLow.mode  = "CCL";
    rangeLow.power = 60.0;
    rangeLow.currentSpec = {
        0.0, 0.8, 0.00001,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCL"
    };
    rangeLow.voltageSpec = {
        0.4, 80.0, 0.0001,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "6V range"
    };
    rangeLow.resistanceSpec = {
        {{ 0.01, 20.0, {0.1, 0.275, 0.0, "S", "CRL"} }},
        16, "1.322mS"
    };
    rangeLow.dynamicSpec = {
        0.00001, 0.01, 0.000001,
        0.00016, 0.08, 0.00001,
        0.000001,
        0.0, 0.8, 0.00001, 0.004,
        "Slew: 0.16A/ms~0.08A/us"
    };
    rangeLow.remark = "60W/CCL";

    // CCM 中檔 (0~8A)
    PowerRangeSpec63600 rangeMid;
    rangeMid.mode  = "CCM";
    rangeMid.power = 60.0;
    rangeMid.currentSpec = {
        0.0, 8.0, 0.0001,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCM"
    };
    rangeMid.voltageSpec = {
        0.4, 80.0, 0.001,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "16V range"
    };
    rangeMid.resistanceSpec = {
        {{ 0.36, 720.0, {0.1, 0.036, 0.0, "S", "CRM"} }},
        16, "1.322mS"
    };
    rangeMid.dynamicSpec = {
        0.00001, 0.01, 0.000001,
        0.0016, 0.8, 0.0001,
        0.000001,
        0.0, 8.0, 0.0001, 0.004,
        "Slew: 1.6A/ms~0.8A/us"
    };
    rangeMid.remark = "60W/CCM";

    // CCH 高檔 (0~80A)
    PowerRangeSpec63600 rangeHigh;
    rangeHigh.mode  = "CCH";
    rangeHigh.power = 400.0;
    rangeHigh.currentSpec = {
        0.0, 80.0, 0.001,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCH"
    };
    rangeHigh.voltageSpec = {
        0.4, 80.0, 0.001,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "80V range"
    };
    rangeHigh.resistanceSpec = {
        {{ 1.45, 2900.0, {0.1, 0.01375, 0.0, "S", "CRH"} }},
        16, "1.322mS"
    };
    rangeHigh.dynamicSpec = {
        0.00001, 0.01, 0.000001,
        0.016, 8.0, 0.001,
        0.000001,
        0.0, 80.0, 0.001, 0.004,
        "Slew: 16A/ms~8A/us"
    };
    rangeHigh.remark = "400W/CCH";

    spec.ranges = { rangeLow, rangeMid, rangeHigh };
    return spec;
}

// ==================== 63640-150-60 規格 (400W, 0~150V, 0~60A) ====================

ChromaLoad63600Spec createChroma6364015060Spec()
{
    ChromaLoad63600Spec spec;
    spec.model = "63640-150-60";

    // CCL 低檔 (0~1A)
    PowerRangeSpec63600 rangeLow;
    rangeLow.mode  = "CCL";
    rangeLow.power = 400.0;
    rangeLow.currentSpec = {
        0.0, 1.0, 0.00002,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCL"
    };
    rangeLow.voltageSpec = {
        1.8, 150.0, 0.001,
        {0.025, 0.025, 0.0, "%F.S.", "0.025%+0.025%F.S."},
        "16V range"
    };
    rangeLow.dynamicSpec = {
        0.00001, 0.01, 0.000001,
        0.0002, 0.1, 0.00002,
        0.000001,
        0.0, 1.0, 0.00002, 0.004,
        "Slew: 0.2A/ms~0.1A/us"
    };
    rangeLow.remark = "400W/CCL";

    // CCM 中檔 (0~6A)
    PowerRangeSpec63600 rangeMid;
    rangeMid.mode  = "CCM";
    rangeMid.power = 400.0;
    rangeMid.currentSpec = {
        0.0, 6.0, 0.0001,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCM"
    };
    rangeMid.voltageSpec = {
        1.8, 150.0, 0.001,
        {0.025, 0.025, 0.0, "%F.S.", "0.025%+0.025%F.S."},
        "80V range"
    };
    rangeMid.dynamicSpec = {
        0.00001, 0.01, 0.000001,
        0.0012, 0.6, 0.0001,
        0.000001,
        0.0, 6.0, 0.0001, 0.004,
        "Slew: 1.2A/ms~0.6A/us"
    };
    rangeMid.remark = "400W/CCM";

    // CCH 高檔 (0~60A)
    PowerRangeSpec63600 rangeHigh;
    rangeHigh.mode  = "CCH";
    rangeHigh.power = 400.0;
    rangeHigh.currentSpec = {
        0.0, 60.0, 0.001,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCH"
    };
    rangeHigh.voltageSpec = {
        1.8, 150.0, 0.01,
        {0.025, 0.025, 0.0, "%F.S.", "0.025%+0.025%F.S."},
        "150V range"
    };
    rangeHigh.dynamicSpec = {
        0.00001, 0.01, 0.000001,
        0.012, 6.0, 0.001,
        0.000001,
        0.0, 60.0, 0.001, 0.004,
        "Slew: 12A/ms~6A/us"
    };
    rangeHigh.remark = "400W/CCH";

    spec.ranges = { rangeLow, rangeMid, rangeHigh };
    return spec;
}

// ==================== 63630-600-15 規格 (600W, 0~600V, 0~15A) ====================

ChromaLoad63600Spec createChroma6363060015Spec()
{
    ChromaLoad63600Spec spec;
    spec.model = "63630-600-15";

    // CCL 低檔 (0~0.15A)
    PowerRangeSpec63600 rangeLow;
    rangeLow.mode  = "CCL";
    rangeLow.power = 90.0;
    rangeLow.currentSpec = {
        0.0, 0.15, 0.000005,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCL"
    };
    rangeLow.voltageSpec = {
        2.0, 600.0, 0.01,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "80V range"
    };
    rangeLow.dynamicSpec = {
        0.00001, 0.01, 0.000001,
        0.000015, 0.015, 0.000005,
        0.000001,
        0.0, 0.15, 0.000005, 0.004,
        "CCL Dynamic"
    };
    rangeLow.remark = "90W/CCL";

    // CCM 中檔 (0~1.5A)
    PowerRangeSpec63600 rangeMid;
    rangeMid.mode  = "CCM";
    rangeMid.power = 90.0;
    rangeMid.currentSpec = {
        0.0, 1.5, 0.00005,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCM"
    };
    rangeMid.voltageSpec = {
        2.0, 600.0, 0.01,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "200V range"
    };
    rangeMid.dynamicSpec = {
        0.00001, 0.01, 0.000001,
        0.00015, 0.15, 0.00005,
        0.000001,
        0.0, 1.5, 0.00005, 0.004,
        "CCM Dynamic"
    };
    rangeMid.remark = "90W/CCM";

    // CCH 高檔 (0~15A)
    PowerRangeSpec63600 rangeHigh;
    rangeHigh.mode  = "CCH";
    rangeHigh.power = 600.0;
    rangeHigh.currentSpec = {
        0.0, 15.0, 0.0005,
        {0.1, 0.1, 0.0, "%F.S.", "0.1%+0.1%F.S."},
        "CCH"
    };
    rangeHigh.voltageSpec = {
        2.0, 600.0, 0.1,
        {0.05, 0.1, 0.0, "%F.S.", "0.05%+0.1%F.S."},
        "600V range"
    };
    rangeHigh.dynamicSpec = {
        0.00001, 0.01, 0.000001,
        0.0015, 1.5, 0.0005,
        0.000001,
        0.0, 15.0, 0.0005, 0.004,
        "CCH Dynamic"
    };
    rangeHigh.remark = "600W/CCH";

    spec.ranges = { rangeLow, rangeMid, rangeHigh };
    return spec;
}
