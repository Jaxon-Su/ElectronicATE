#include "chromaload63200aspec.h"
#include <QMap>
#include <QString>
#include <functional>
#include <optional>
#include <QDebug>

// ==================== 規格映射表 ====================

static QMap<QString, std::function<ChromaLoad63200ASpec()>> createSpecMap63200A = {
    // 150V 系列
    { "63202A-150-200",  createChroma63202A_150_200Spec  },
    { "63203A-150-300",  createChroma63203A_150_300Spec  },
    { "63204A-150-400",  createChroma63204A_150_400Spec  },
    { "63205A-150-500",  createChroma63205A_150_500Spec  },
    { "63206A-150-600",  createChroma63206A_150_600Spec  },
    { "63208A-150-800",  createChroma63208A_150_800Spec  },
    // 600V 系列
    { "63202A-600-140",  createChroma63202A_600_140Spec  },
    { "63205A-600-350",  createChroma63205A_600_350Spec  },
    { "63206A-600-420",  createChroma63206A_600_420Spec  },
    { "63224A-600-1680", createChroma63224A_600_1680Spec },
    // 1200V 系列
    { "63202A-1200-80",  createChroma63202A_1200_80Spec  },
    { "63204A-1200-160", createChroma63204A_1200_160Spec },
    { "63205A-1200-200", createChroma63205A_1200_200Spec },
    { "63206A-1200-240", createChroma63206A_1200_240Spec },
    { "63208A-1200-320", createChroma63208A_1200_320Spec },
    };

// ==================== findPowerRange63200A ====================

std::optional<PowerRangeSpec63200A> findPowerRange63200A(const QString& subModel,
                                                         double currval)
{
    if (!createSpecMap63200A.contains(subModel)) {
        qWarning() << "[Chroma63200ASpec] Unknown subModel:" << subModel;
        return std::nullopt;
    }

    ChromaLoad63200ASpec spec = createSpecMap63200A[subModel]();

    for (const auto& range : spec.ranges) {
        if (currval <= range.currentSpec.maxCurrent) {
            return range;
        }
    }

    // 超出最大電流範圍 → 回傳 CCH，讓硬體自行保護
    if (!spec.ranges.isEmpty()) {
        qWarning() << "[Chroma63200ASpec] Current" << currval
                   << "A exceeds all ranges for" << subModel
                   << "- using CCH (last range)";
        return spec.ranges.last();
    }

    return std::nullopt;
}

// ==================== selectOptimalLoadMode63200A ====================

QString selectOptimalLoadMode63200A(const QString& subModel,
                                    double current,
                                    double voltage)
{
    const double MARGIN = 0.95;
    double expectedPower = current * voltage;

    auto rangeOpt = findPowerRange63200A(subModel, current / MARGIN);
    if (!rangeOpt) {
        qWarning() << "[Chroma63200ASpec] Unknown subModel:" << subModel << "- fallback CCH";
        return "CCH";
    }

    const PowerRangeSpec63200A& range = *rangeOpt;

    bool currentOK = (current <= range.currentSpec.maxCurrent * MARGIN);
    bool powerOK   = (expectedPower <= range.power * MARGIN);
    bool voltageOK = (voltage >= range.voltageSpec.minVoltage &&
                      voltage <= range.voltageSpec.maxVoltage);

    if (!currentOK || !powerOK || !voltageOK) {
        qWarning() << "[Chroma63200ASpec] Range" << range.mode
                   << "constraints not met:"
                   << "currentOK=" << currentOK
                   << "powerOK="   << powerOK
                   << "voltageOK=" << voltageOK
                   << "- fallback CCH";
        return "CCH";
    }

    qDebug() << "[Chroma63200ASpec] Selected mode:" << range.mode
             << "SubModel:" << subModel
             << "I=" << current << "A"
             << "V=" << voltage << "V"
             << "P=" << expectedPower << "W";

    return range.mode;
}

// ==================== selectOptimalCVMode63200A ====================

static QString toCVMode63200A(const QString& ccMode)
{
    if (ccMode == "CCL") return "CVL";
    if (ccMode == "CCM") return "CVM";
    return "CVH";
}

QString selectOptimalCVMode63200A(const QString& subModel,
                                  double voltage,
                                  double currentLimit)
{
    if (!createSpecMap63200A.contains(subModel)) {
        qWarning() << "[Chroma63200ASpec] Unknown subModel:" << subModel << "- fallback CVH";
        return "CVH";
    }

    ChromaLoad63200ASpec spec = createSpecMap63200A[subModel]();
    const double expectedPower = voltage * currentLimit;

    for (const auto& range : spec.ranges) {
        const bool voltageOK = (voltage >= range.voltageSpec.minVoltage &&
                                voltage <= range.voltageSpec.maxVoltage);
        const bool currentOK = (currentLimit <= range.currentSpec.maxCurrent);
        const bool powerOK = (expectedPower <= range.power);

        if (voltageOK && currentOK && powerOK) {
            const QString mode = toCVMode63200A(range.mode);
            qDebug() << "[Chroma63200ASpec] Selected CV mode:" << mode
                     << "SubModel:" << subModel
                     << "V=" << voltage << "V"
                     << "Ilimit=" << currentLimit << "A"
                     << "P=" << expectedPower << "W";
            return mode;
        }
    }

    qWarning() << "[Chroma63200ASpec] CV constraints not met:"
               << "SubModel=" << subModel
               << "V=" << voltage << "V"
               << "Ilimit=" << currentLimit << "A"
               << "P=" << expectedPower << "W"
               << "- fallback CVH";
    return "CVH";
}

// ==================== 輔助巨集：建立 150V 型號通用電壓規格 ====================
// 150V 型號電壓量測檔位：Low=16V, Middle=80V, High=150V（手冊 p.23）

static VoltageRangeSpec63200A makeVoltRange150(const QString& rangeLabel)
{
    VoltageRangeSpec63200A v;
    if (rangeLabel == "CCL") {
        v.minVoltage = 1.8; v.maxVoltage = 16.0;  v.resolution = 0.0001;
    } else if (rangeLabel == "CCM") {
        v.minVoltage = 1.8; v.maxVoltage = 80.0;  v.resolution = 0.0005;
    } else { // CCH
        v.minVoltage = 1.8; v.maxVoltage = 150.0; v.resolution = 0.001;
    }
    v.accuracy = {0.025, 0.025, 0.0, "%F.S.", "0.025%+0.025%F.S."};
    v.remark   = rangeLabel + " Vrange";
    return v;
}

// 600V 型號電壓量測檔位：Low=80V, Middle=150V, High=600V（手冊 p.27）
static VoltageRangeSpec63200A makeVoltRange600(const QString& rangeLabel)
{
    VoltageRangeSpec63200A v;
    if (rangeLabel == "CCL") {
        v.minVoltage = 14.0; v.maxVoltage = 80.0;  v.resolution = 0.0005;
    } else if (rangeLabel == "CCM") {
        v.minVoltage = 14.0; v.maxVoltage = 150.0; v.resolution = 0.001;
    } else { // CCH
        v.minVoltage = 14.0; v.maxVoltage = 600.0; v.resolution = 0.005;
    }
    v.accuracy = {0.025, 0.025, 0.0, "%F.S.", "0.025%+0.025%F.S."};
    v.remark   = rangeLabel + " Vrange";
    return v;
}

// 1200V 型號電壓量測檔位：Low=150V, Middle=600V, High=1200V（手冊 p.31）
static VoltageRangeSpec63200A makeVoltRange1200(const QString& rangeLabel)
{
    VoltageRangeSpec63200A v;
    if (rangeLabel == "CCL") {
        v.minVoltage = 20.0; v.maxVoltage = 150.0;  v.resolution = 0.001;
    } else if (rangeLabel == "CCM") {
        v.minVoltage = 20.0; v.maxVoltage = 600.0;  v.resolution = 0.005;
    } else { // CCH
        v.minVoltage = 20.0; v.maxVoltage = 1200.0; v.resolution = 0.01;
    }
    v.accuracy = {0.025, 0.025, 0.0, "%F.S.", "0.025%+0.025%F.S."};
    v.remark   = rangeLabel + " Vrange";
    return v;
}

// ==================== 150V 系列 ====================

// -------- 63202A-150-200 (2kW, 0~150V, 0~200A) --------
// Ranges: 20/100/200A  Resolution: 0.2/1/2 mA  Slew: 0.2mA/μs~2A/μs / 1~7 / 2~14 A/μs
ChromaLoad63200ASpec createChroma63202A_150_200Spec()
{
    ChromaLoad63200ASpec spec;
    spec.model = "63202A-150-200";

    PowerRangeSpec63200A rangeLow;
    rangeLow.mode  = "CCL";
    rangeLow.power = 320.0;  // 20A × 16V
    rangeLow.currentSpec = { 0.0, 20.0, 0.0002,
                            {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCL" };
    rangeLow.voltageSpec = makeVoltRange150("CCL");
    rangeLow.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.0002, 2.0, 0.0002, 0.0,
                            0.0, 20.0, 0.0002, 0.002, "Slew: 0.2mA/μs~2A/μs" };
    rangeLow.remark = "320W/CCL";

    PowerRangeSpec63200A rangeMid;
    rangeMid.mode  = "CCM";
    rangeMid.power = 2000.0;
    rangeMid.currentSpec = { 0.0, 100.0, 0.001,
                            {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCM" };
    rangeMid.voltageSpec = makeVoltRange150("CCM");
    rangeMid.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.001, 7.0, 0.001, 0.0,
                            0.0, 100.0, 0.001, 0.002, "Slew: 1mA/μs~7A/μs" };
    rangeMid.remark = "2000W/CCM";

    PowerRangeSpec63200A rangeHigh;
    rangeHigh.mode  = "CCH";
    rangeHigh.power = 2000.0;
    rangeHigh.currentSpec = { 0.0, 200.0, 0.002,
                             {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCH" };
    rangeHigh.voltageSpec = makeVoltRange150("CCH");
    rangeHigh.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                             0.002, 14.0, 0.002, 0.0,
                             0.0, 200.0, 0.002, 0.002, "Slew: 2mA/μs~14A/μs" };
    rangeHigh.remark = "2000W/CCH";

    spec.ranges = { rangeLow, rangeMid, rangeHigh };
    return spec;
}

// -------- 63203A-150-300 (3kW, 0~150V, 0~300A) --------
// Ranges: 30/150/300A  Resolution: 0.2/1/2 mA  Slew: 0.2~3/1~10.5/2~21 A/μs
ChromaLoad63200ASpec createChroma63203A_150_300Spec()
{
    ChromaLoad63200ASpec spec;
    spec.model = "63203A-150-300";

    PowerRangeSpec63200A rangeLow;
    rangeLow.mode  = "CCL";
    rangeLow.power = 480.0;
    rangeLow.currentSpec = { 0.0, 30.0, 0.0002,
                            {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCL" };
    rangeLow.voltageSpec = makeVoltRange150("CCL");
    rangeLow.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.0002, 3.0, 0.0002, 0.0,
                            0.0, 30.0, 0.0002, 0.002, "Slew: 0.2mA/μs~3A/μs" };
    rangeLow.remark = "480W/CCL";

    PowerRangeSpec63200A rangeMid;
    rangeMid.mode  = "CCM";
    rangeMid.power = 3000.0;
    rangeMid.currentSpec = { 0.0, 150.0, 0.001,
                            {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCM" };
    rangeMid.voltageSpec = makeVoltRange150("CCM");
    rangeMid.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.001, 10.5, 0.001, 0.0,
                            0.0, 150.0, 0.001, 0.002, "Slew: 1mA/μs~10.5A/μs" };
    rangeMid.remark = "3000W/CCM";

    PowerRangeSpec63200A rangeHigh;
    rangeHigh.mode  = "CCH";
    rangeHigh.power = 3000.0;
    rangeHigh.currentSpec = { 0.0, 300.0, 0.002,
                             {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCH" };
    rangeHigh.voltageSpec = makeVoltRange150("CCH");
    rangeHigh.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                             0.002, 21.0, 0.002, 0.0,
                             0.0, 300.0, 0.002, 0.002, "Slew: 2mA/μs~21A/μs" };
    rangeHigh.remark = "3000W/CCH";

    spec.ranges = { rangeLow, rangeMid, rangeHigh };
    return spec;
}

// -------- 63204A-150-400 (4kW, 0~150V, 0~400A) --------
// Ranges: 40/200/400A  Resolution: 0.4/2/4 mA  Slew: 0.5~4/2~14/5~28 A/μs
ChromaLoad63200ASpec createChroma63204A_150_400Spec()
{
    ChromaLoad63200ASpec spec;
    spec.model = "63204A-150-400";

    PowerRangeSpec63200A rangeLow;
    rangeLow.mode  = "CCL";
    rangeLow.power = 640.0;
    rangeLow.currentSpec = { 0.0, 40.0, 0.0004,
                            {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCL" };
    rangeLow.voltageSpec = makeVoltRange150("CCL");
    rangeLow.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.0005, 4.0, 0.0004, 0.0,
                            0.0, 40.0, 0.0004, 0.002, "Slew: 0.5mA/μs~4A/μs" };
    rangeLow.remark = "640W/CCL";

    PowerRangeSpec63200A rangeMid;
    rangeMid.mode  = "CCM";
    rangeMid.power = 4000.0;
    rangeMid.currentSpec = { 0.0, 200.0, 0.002,
                            {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCM" };
    rangeMid.voltageSpec = makeVoltRange150("CCM");
    rangeMid.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.002, 14.0, 0.002, 0.0,
                            0.0, 200.0, 0.002, 0.002, "Slew: 2mA/μs~14A/μs" };
    rangeMid.remark = "4000W/CCM";

    PowerRangeSpec63200A rangeHigh;
    rangeHigh.mode  = "CCH";
    rangeHigh.power = 4000.0;
    rangeHigh.currentSpec = { 0.0, 400.0, 0.004,
                             {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCH" };
    rangeHigh.voltageSpec = makeVoltRange150("CCH");
    rangeHigh.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                             0.005, 28.0, 0.004, 0.0,
                             0.0, 400.0, 0.004, 0.002, "Slew: 5mA/μs~28A/μs" };
    rangeHigh.remark = "4000W/CCH";

    spec.ranges = { rangeLow, rangeMid, rangeHigh };
    return spec;
}

// -------- 63205A-150-500 (5kW, 0~150V, 0~500A) --------
// Ranges: 50/250/500A  Resolution: 0.5/2/5 mA  Slew: 0.5~5/2~17.5/5~35 A/μs
ChromaLoad63200ASpec createChroma63205A_150_500Spec()
{
    ChromaLoad63200ASpec spec;
    spec.model = "63205A-150-500";

    PowerRangeSpec63200A rangeLow;
    rangeLow.mode  = "CCL";
    rangeLow.power = 800.0;
    rangeLow.currentSpec = { 0.0, 50.0, 0.0005,
                            {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCL" };
    rangeLow.voltageSpec = makeVoltRange150("CCL");
    rangeLow.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.0005, 5.0, 0.0005, 0.0,
                            0.0, 50.0, 0.0005, 0.002, "Slew: 0.5mA/μs~5A/μs" };
    rangeLow.remark = "800W/CCL";

    PowerRangeSpec63200A rangeMid;
    rangeMid.mode  = "CCM";
    rangeMid.power = 5000.0;
    rangeMid.currentSpec = { 0.0, 250.0, 0.002,
                            {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCM" };
    rangeMid.voltageSpec = makeVoltRange150("CCM");
    rangeMid.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.002, 17.5, 0.002, 0.0,
                            0.0, 250.0, 0.002, 0.002, "Slew: 2mA/μs~17.5A/μs" };
    rangeMid.remark = "5000W/CCM";

    PowerRangeSpec63200A rangeHigh;
    rangeHigh.mode  = "CCH";
    rangeHigh.power = 5000.0;
    rangeHigh.currentSpec = { 0.0, 500.0, 0.005,
                             {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCH" };
    rangeHigh.voltageSpec = makeVoltRange150("CCH");
    rangeHigh.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                             0.005, 35.0, 0.005, 0.0,
                             0.0, 500.0, 0.005, 0.002, "Slew: 5mA/μs~35A/μs" };
    rangeHigh.remark = "5000W/CCH";

    spec.ranges = { rangeLow, rangeMid, rangeHigh };
    return spec;
}

// -------- 63206A-150-600 (6kW, 0~150V, 0~600A) --------
// Ranges: 60/300/600A  Resolution: 0.5/2/5 mA  Slew: 0.5~6/2~21/5~42 A/μs
ChromaLoad63200ASpec createChroma63206A_150_600Spec()
{
    ChromaLoad63200ASpec spec;
    spec.model = "63206A-150-600";

    PowerRangeSpec63200A rangeLow;
    rangeLow.mode  = "CCL";
    rangeLow.power = 960.0;
    rangeLow.currentSpec = { 0.0, 60.0, 0.0005,
                            {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCL" };
    rangeLow.voltageSpec = makeVoltRange150("CCL");
    rangeLow.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.0005, 6.0, 0.0005, 0.0,
                            0.0, 60.0, 0.0005, 0.002, "Slew: 0.5mA/μs~6A/μs" };
    rangeLow.remark = "960W/CCL";

    PowerRangeSpec63200A rangeMid;
    rangeMid.mode  = "CCM";
    rangeMid.power = 6000.0;
    rangeMid.currentSpec = { 0.0, 300.0, 0.002,
                            {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCM" };
    rangeMid.voltageSpec = makeVoltRange150("CCM");
    rangeMid.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.002, 21.0, 0.002, 0.0,
                            0.0, 300.0, 0.002, 0.002, "Slew: 2mA/μs~21A/μs" };
    rangeMid.remark = "6000W/CCM";

    PowerRangeSpec63200A rangeHigh;
    rangeHigh.mode  = "CCH";
    rangeHigh.power = 6000.0;
    rangeHigh.currentSpec = { 0.0, 600.0, 0.005,
                             {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCH" };
    rangeHigh.voltageSpec = makeVoltRange150("CCH");
    rangeHigh.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                             0.005, 42.0, 0.005, 0.0,
                             0.0, 600.0, 0.005, 0.002, "Slew: 5mA/μs~42A/μs" };
    rangeHigh.remark = "6000W/CCH";

    spec.ranges = { rangeLow, rangeMid, rangeHigh };
    return spec;
}

// -------- 63208A-150-800 (8kW, 0~150V, 0~800A) --------
// Ranges: 80/400/800A  Resolution: 1/5/10 mA  Slew: 1~8/5~24/10~48 A/μs
ChromaLoad63200ASpec createChroma63208A_150_800Spec()
{
    ChromaLoad63200ASpec spec;
    spec.model = "63208A-150-800";

    PowerRangeSpec63200A rangeLow;
    rangeLow.mode  = "CCL";
    rangeLow.power = 1280.0;
    rangeLow.currentSpec = { 0.0, 80.0, 0.001,
                            {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCL" };
    rangeLow.voltageSpec = makeVoltRange150("CCL");
    rangeLow.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.001, 8.0, 0.001, 0.0,
                            0.0, 80.0, 0.001, 0.002, "Slew: 1mA/μs~8A/μs" };
    rangeLow.remark = "1280W/CCL";

    PowerRangeSpec63200A rangeMid;
    rangeMid.mode  = "CCM";
    rangeMid.power = 8000.0;
    rangeMid.currentSpec = { 0.0, 400.0, 0.005,
                            {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCM" };
    rangeMid.voltageSpec = makeVoltRange150("CCM");
    rangeMid.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.005, 24.0, 0.005, 0.0,
                            0.0, 400.0, 0.005, 0.002, "Slew: 5mA/μs~24A/μs" };
    rangeMid.remark = "8000W/CCM";

    PowerRangeSpec63200A rangeHigh;
    rangeHigh.mode  = "CCH";
    rangeHigh.power = 8000.0;
    rangeHigh.currentSpec = { 0.0, 800.0, 0.01,
                             {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCH" };
    rangeHigh.voltageSpec = makeVoltRange150("CCH");
    rangeHigh.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                             0.01, 48.0, 0.01, 0.0,
                             0.0, 800.0, 0.01, 0.002, "Slew: 10mA/μs~48A/μs" };
    rangeHigh.remark = "8000W/CCH";

    spec.ranges = { rangeLow, rangeMid, rangeHigh };
    return spec;
}

// ==================== 600V 系列 ====================

// -------- 63202A-600-140 (2kW, 0~600V, 0~140A) --------
// Ranges: 14/70/140A  Resolution: 0.2/1/2 mA  Slew: 0.2~0.6/1~3/2~6 A/μs
ChromaLoad63200ASpec createChroma63202A_600_140Spec()
{
    ChromaLoad63200ASpec spec;
    spec.model = "63202A-600-140";

    PowerRangeSpec63200A rangeLow;
    rangeLow.mode  = "CCL";
    rangeLow.power = 1120.0;
    rangeLow.currentSpec = { 0.0, 14.0, 0.0002,
                            {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCL" };
    rangeLow.voltageSpec = makeVoltRange600("CCL");
    rangeLow.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.0002, 0.6, 0.0002, 0.0,
                            0.0, 14.0, 0.0002, 0.002, "Slew: 0.2mA/μs~0.6A/μs" };
    rangeLow.remark = "1120W/CCL";

    PowerRangeSpec63200A rangeMid;
    rangeMid.mode  = "CCM";
    rangeMid.power = 2000.0;
    rangeMid.currentSpec = { 0.0, 70.0, 0.001,
                            {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCM" };
    rangeMid.voltageSpec = makeVoltRange600("CCM");
    rangeMid.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.001, 3.0, 0.001, 0.0,
                            0.0, 70.0, 0.001, 0.002, "Slew: 1mA/μs~3A/μs" };
    rangeMid.remark = "2000W/CCM";

    PowerRangeSpec63200A rangeHigh;
    rangeHigh.mode  = "CCH";
    rangeHigh.power = 2000.0;
    rangeHigh.currentSpec = { 0.0, 140.0, 0.002,
                             {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCH" };
    rangeHigh.voltageSpec = makeVoltRange600("CCH");
    rangeHigh.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                             0.002, 6.0, 0.002, 0.0,
                             0.0, 140.0, 0.002, 0.002, "Slew: 2mA/μs~6A/μs" };
    rangeHigh.remark = "2000W/CCH";

    spec.ranges = { rangeLow, rangeMid, rangeHigh };
    return spec;
}

// -------- 63205A-600-350 (5kW, 0~600V, 0~350A) --------
// Ranges: 35/175/350A  Resolution: 0.4/2/4 mA  Slew: 0.4~1.5/2~7.5/4~15 A/μs
ChromaLoad63200ASpec createChroma63205A_600_350Spec()
{
    ChromaLoad63200ASpec spec;
    spec.model = "63205A-600-350";

    PowerRangeSpec63200A rangeLow;
    rangeLow.mode  = "CCL";
    rangeLow.power = 2800.0;
    rangeLow.currentSpec = { 0.0, 35.0, 0.0004,
                            {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCL" };
    rangeLow.voltageSpec = makeVoltRange600("CCL");
    rangeLow.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.0004, 1.5, 0.0004, 0.0,
                            0.0, 35.0, 0.0004, 0.002, "Slew: 0.4mA/μs~1.5A/μs" };
    rangeLow.remark = "2800W/CCL";

    PowerRangeSpec63200A rangeMid;
    rangeMid.mode  = "CCM";
    rangeMid.power = 5000.0;
    rangeMid.currentSpec = { 0.0, 175.0, 0.002,
                            {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCM" };
    rangeMid.voltageSpec = makeVoltRange600("CCM");
    rangeMid.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.002, 7.5, 0.002, 0.0,
                            0.0, 175.0, 0.002, 0.002, "Slew: 2mA/μs~7.5A/μs" };
    rangeMid.remark = "5000W/CCM";

    PowerRangeSpec63200A rangeHigh;
    rangeHigh.mode  = "CCH";
    rangeHigh.power = 5000.0;
    rangeHigh.currentSpec = { 0.0, 350.0, 0.004,
                             {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCH" };
    rangeHigh.voltageSpec = makeVoltRange600("CCH");
    rangeHigh.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                             0.004, 15.0, 0.004, 0.0,
                             0.0, 350.0, 0.004, 0.002, "Slew: 4mA/μs~15A/μs" };
    rangeHigh.remark = "5000W/CCH";

    spec.ranges = { rangeLow, rangeMid, rangeHigh };
    return spec;
}

// -------- 63206A-600-420 (6kW, 0~600V, 0~420A) --------
// Ranges: 42/210/420A  Resolution: 0.4/2/4 mA  Slew: 0.4~1.8/2~9/4~18 A/μs
ChromaLoad63200ASpec createChroma63206A_600_420Spec()
{
    ChromaLoad63200ASpec spec;
    spec.model = "63206A-600-420";

    PowerRangeSpec63200A rangeLow;
    rangeLow.mode  = "CCL";
    rangeLow.power = 3360.0;
    rangeLow.currentSpec = { 0.0, 42.0, 0.0004,
                            {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCL" };
    rangeLow.voltageSpec = makeVoltRange600("CCL");
    rangeLow.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.0004, 1.8, 0.0004, 0.0,
                            0.0, 42.0, 0.0004, 0.002, "Slew: 0.4mA/μs~1.8A/μs" };
    rangeLow.remark = "3360W/CCL";

    PowerRangeSpec63200A rangeMid;
    rangeMid.mode  = "CCM";
    rangeMid.power = 6000.0;
    rangeMid.currentSpec = { 0.0, 210.0, 0.002,
                            {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCM" };
    rangeMid.voltageSpec = makeVoltRange600("CCM");
    rangeMid.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.002, 9.0, 0.002, 0.0,
                            0.0, 210.0, 0.002, 0.002, "Slew: 2mA/μs~9A/μs" };
    rangeMid.remark = "6000W/CCM";

    PowerRangeSpec63200A rangeHigh;
    rangeHigh.mode  = "CCH";
    rangeHigh.power = 6000.0;
    rangeHigh.currentSpec = { 0.0, 420.0, 0.004,
                             {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCH" };
    rangeHigh.voltageSpec = makeVoltRange600("CCH");
    rangeHigh.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                             0.004, 18.0, 0.004, 0.0,
                             0.0, 420.0, 0.004, 0.002, "Slew: 4mA/μs~18A/μs" };
    rangeHigh.remark = "6000W/CCH";

    spec.ranges = { rangeLow, rangeMid, rangeHigh };
    return spec;
}

// -------- 63224A-600-1680 (24kW, 0~600V, 0~1680A) --------
// Ranges: 168/840/1680A  Resolution: 2/10/20 mA  Slew: 2mA~3.6/10mA~18/20mA~36 A/us
ChromaLoad63200ASpec createChroma63224A_600_1680Spec()
{
    ChromaLoad63200ASpec spec;
    spec.model = "63224A-600-1680";

    PowerRangeSpec63200A rangeLow;
    rangeLow.mode  = "CCL";
    rangeLow.power = 13440.0;
    rangeLow.currentSpec = { 0.0, 168.0, 0.002,
                            {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCL" };
    rangeLow.voltageSpec = makeVoltRange600("CCL");
    rangeLow.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.002, 3.6, 0.002, 0.0,
                            0.0, 168.0, 0.002, 0.002, "Slew: 2mA/us~3.6A/us" };
    rangeLow.remark = "13440W/CCL";

    PowerRangeSpec63200A rangeMid;
    rangeMid.mode  = "CCM";
    rangeMid.power = 24000.0;
    rangeMid.currentSpec = { 0.0, 840.0, 0.010,
                            {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCM" };
    rangeMid.voltageSpec = makeVoltRange600("CCM");
    rangeMid.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.010, 18.0, 0.010, 0.0,
                            0.0, 840.0, 0.010, 0.002, "Slew: 10mA/us~18A/us" };
    rangeMid.remark = "24000W/CCM";

    PowerRangeSpec63200A rangeHigh;
    rangeHigh.mode  = "CCH";
    rangeHigh.power = 24000.0;
    rangeHigh.currentSpec = { 0.0, 1680.0, 0.020,
                             {0.05, 0.05, 0.0, "%F.S.", "0.05%+0.05%F.S."}, "CCH" };
    rangeHigh.voltageSpec = makeVoltRange600("CCH");
    rangeHigh.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                             0.020, 36.0, 0.020, 0.0,
                             0.0, 1680.0, 0.020, 0.002, "Slew: 20mA/us~36A/us" };
    rangeHigh.remark = "24000W/CCH";

    spec.ranges = { rangeLow, rangeMid, rangeHigh };
    return spec;
}

// ==================== 1200V 系列 ====================

// -------- 63202A-1200-80 (2kW, 0~1200V, 0~80A) --------
// Ranges: 8/40/80A  Resolution: 0.1/0.5/1 mA  Slew: 0.1~0.4/0.5~2/1~4 A/μs
// Note: 1200V 型號 CC accuracy 0.04%+0.06%F.S.（手冊 p.31）
ChromaLoad63200ASpec createChroma63202A_1200_80Spec()
{
    ChromaLoad63200ASpec spec;
    spec.model = "63202A-1200-80";

    PowerRangeSpec63200A rangeLow;
    rangeLow.mode  = "CCL";
    rangeLow.power = 1200.0;
    rangeLow.currentSpec = { 0.0, 8.0, 0.0001,
                            {0.04, 0.06, 0.0, "%F.S.", "0.04%+0.06%F.S."}, "CCL" };
    rangeLow.voltageSpec = makeVoltRange1200("CCL");
    rangeLow.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.0001, 0.4, 0.0001, 0.0,
                            0.0, 8.0, 0.0001, 0.002, "Slew: 0.1mA/μs~0.4A/μs" };
    rangeLow.remark = "1200W/CCL";

    PowerRangeSpec63200A rangeMid;
    rangeMid.mode  = "CCM";
    rangeMid.power = 2000.0;
    rangeMid.currentSpec = { 0.0, 40.0, 0.0005,
                            {0.04, 0.06, 0.0, "%F.S.", "0.04%+0.06%F.S."}, "CCM" };
    rangeMid.voltageSpec = makeVoltRange1200("CCM");
    rangeMid.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.0005, 2.0, 0.0005, 0.0,
                            0.0, 40.0, 0.0005, 0.002, "Slew: 0.5mA/μs~2A/μs" };
    rangeMid.remark = "2000W/CCM";

    PowerRangeSpec63200A rangeHigh;
    rangeHigh.mode  = "CCH";
    rangeHigh.power = 2000.0;
    rangeHigh.currentSpec = { 0.0, 80.0, 0.001,
                             {0.04, 0.06, 0.0, "%F.S.", "0.04%+0.06%F.S."}, "CCH" };
    rangeHigh.voltageSpec = makeVoltRange1200("CCH");
    rangeHigh.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                             0.001, 4.0, 0.001, 0.0,
                             0.0, 80.0, 0.001, 0.002, "Slew: 1mA/μs~4A/μs" };
    rangeHigh.remark = "2000W/CCH";

    spec.ranges = { rangeLow, rangeMid, rangeHigh };
    return spec;
}

// -------- 63204A-1200-160 (4kW, 0~1200V, 0~160A) --------
// Ranges: 16/80/160A  Resolution: 0.2/1/2 mA  Slew: 0.2~0.8/1~4/2~8 A/μs
// Note: 1200V 型號 CC accuracy 0.04%+0.06%F.S.（手冊 p.31）
ChromaLoad63200ASpec createChroma63204A_1200_160Spec()
{
    ChromaLoad63200ASpec spec;
    spec.model = "63204A-1200-160";

    PowerRangeSpec63200A rangeLow;
    rangeLow.mode  = "CCL";
    rangeLow.power = 2400.0;
    rangeLow.currentSpec = { 0.0, 16.0, 0.0002,
                            {0.04, 0.06, 0.0, "%F.S.", "0.04%+0.06%F.S."}, "CCL" };
    rangeLow.voltageSpec = makeVoltRange1200("CCL");
    rangeLow.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.0002, 0.8, 0.0002, 0.0,
                            0.0, 16.0, 0.0002, 0.002, "Slew: 0.2mA/μs~0.8A/μs" };
    rangeLow.remark = "2400W/CCL";

    PowerRangeSpec63200A rangeMid;
    rangeMid.mode  = "CCM";
    rangeMid.power = 4000.0;
    rangeMid.currentSpec = { 0.0, 80.0, 0.001,
                            {0.04, 0.06, 0.0, "%F.S.", "0.04%+0.06%F.S."}, "CCM" };
    rangeMid.voltageSpec = makeVoltRange1200("CCM");
    rangeMid.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.001, 4.0, 0.001, 0.0,
                            0.0, 80.0, 0.001, 0.002, "Slew: 1mA/μs~4A/μs" };
    rangeMid.remark = "4000W/CCM";

    PowerRangeSpec63200A rangeHigh;
    rangeHigh.mode  = "CCH";
    rangeHigh.power = 4000.0;
    rangeHigh.currentSpec = { 0.0, 160.0, 0.002,
                             {0.04, 0.06, 0.0, "%F.S.", "0.04%+0.06%F.S."}, "CCH" };
    rangeHigh.voltageSpec = makeVoltRange1200("CCH");
    rangeHigh.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                             0.002, 8.0, 0.002, 0.0,
                             0.0, 160.0, 0.002, 0.002, "Slew: 2mA/μs~8A/μs" };
    rangeHigh.remark = "4000W/CCH";

    spec.ranges = { rangeLow, rangeMid, rangeHigh };
    return spec;
}

// -------- 63205A-1200-200 (5kW, 0~1200V, 0~200A) --------
// Ranges: 20/100/200A  Resolution: 0.2/1/2 mA  Slew: 0.2~1/1~5/2~10 A/μs
ChromaLoad63200ASpec createChroma63205A_1200_200Spec()
{
    ChromaLoad63200ASpec spec;
    spec.model = "63205A-1200-200";

    PowerRangeSpec63200A rangeLow;
    rangeLow.mode  = "CCL";
    rangeLow.power = 3000.0;
    rangeLow.currentSpec = { 0.0, 20.0, 0.0002,
                            {0.04, 0.06, 0.0, "%F.S.", "0.04%+0.06%F.S."}, "CCL" };
    rangeLow.voltageSpec = makeVoltRange1200("CCL");
    rangeLow.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.0002, 1.0, 0.0002, 0.0,
                            0.0, 20.0, 0.0002, 0.002, "Slew: 0.2mA/μs~1A/μs" };
    rangeLow.remark = "3000W/CCL";

    PowerRangeSpec63200A rangeMid;
    rangeMid.mode  = "CCM";
    rangeMid.power = 5000.0;
    rangeMid.currentSpec = { 0.0, 100.0, 0.001,
                            {0.04, 0.06, 0.0, "%F.S.", "0.04%+0.06%F.S."}, "CCM" };
    rangeMid.voltageSpec = makeVoltRange1200("CCM");
    rangeMid.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.001, 5.0, 0.001, 0.0,
                            0.0, 100.0, 0.001, 0.002, "Slew: 1mA/μs~5A/μs" };
    rangeMid.remark = "5000W/CCM";

    PowerRangeSpec63200A rangeHigh;
    rangeHigh.mode  = "CCH";
    rangeHigh.power = 5000.0;
    rangeHigh.currentSpec = { 0.0, 200.0, 0.002,
                             {0.04, 0.06, 0.0, "%F.S.", "0.04%+0.06%F.S."}, "CCH" };
    rangeHigh.voltageSpec = makeVoltRange1200("CCH");
    rangeHigh.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                             0.002, 10.0, 0.002, 0.0,
                             0.0, 200.0, 0.002, 0.002, "Slew: 2mA/μs~10A/μs" };
    rangeHigh.remark = "5000W/CCH";

    spec.ranges = { rangeLow, rangeMid, rangeHigh };
    return spec;
}

// -------- 63206A-1200-240 (6kW, 0~1200V, 0~240A) --------
// Ranges: 24/120/240A  Resolution: 0.2/1/2 mA  Slew: 0.2~1.2/1~6/2~12 A/μs
ChromaLoad63200ASpec createChroma63206A_1200_240Spec()
{
    ChromaLoad63200ASpec spec;
    spec.model = "63206A-1200-240";

    PowerRangeSpec63200A rangeLow;
    rangeLow.mode  = "CCL";
    rangeLow.power = 3600.0;
    rangeLow.currentSpec = { 0.0, 24.0, 0.0002,
                            {0.04, 0.06, 0.0, "%F.S.", "0.04%+0.06%F.S."}, "CCL" };
    rangeLow.voltageSpec = makeVoltRange1200("CCL");
    rangeLow.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.0002, 1.2, 0.0002, 0.0,
                            0.0, 24.0, 0.0002, 0.002, "Slew: 0.2mA/μs~1.2A/μs" };
    rangeLow.remark = "3600W/CCL";

    PowerRangeSpec63200A rangeMid;
    rangeMid.mode  = "CCM";
    rangeMid.power = 6000.0;
    rangeMid.currentSpec = { 0.0, 120.0, 0.001,
                            {0.04, 0.06, 0.0, "%F.S.", "0.04%+0.06%F.S."}, "CCM" };
    rangeMid.voltageSpec = makeVoltRange1200("CCM");
    rangeMid.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.001, 6.0, 0.001, 0.0,
                            0.0, 120.0, 0.001, 0.002, "Slew: 1mA/μs~6A/μs" };
    rangeMid.remark = "6000W/CCM";

    PowerRangeSpec63200A rangeHigh;
    rangeHigh.mode  = "CCH";
    rangeHigh.power = 6000.0;
    rangeHigh.currentSpec = { 0.0, 240.0, 0.002,
                             {0.04, 0.06, 0.0, "%F.S.", "0.04%+0.06%F.S."}, "CCH" };
    rangeHigh.voltageSpec = makeVoltRange1200("CCH");
    rangeHigh.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                             0.002, 12.0, 0.002, 0.0,
                             0.0, 240.0, 0.002, 0.002, "Slew: 2mA/μs~12A/μs" };
    rangeHigh.remark = "6000W/CCH";

    spec.ranges = { rangeLow, rangeMid, rangeHigh };
    return spec;
}

// -------- 63208A-1200-320 (8kW, 0~1200V, 0~320A) --------
// Ranges: 32/160/320A  Resolution: 0.4/2/4 mA  Slew: 0.4~1.2/2~6/4~12 A/us
ChromaLoad63200ASpec createChroma63208A_1200_320Spec()
{
    ChromaLoad63200ASpec spec;
    spec.model = "63208A-1200-320";

    PowerRangeSpec63200A rangeLow;
    rangeLow.mode  = "CCL";
    rangeLow.power = 4800.0;
    rangeLow.currentSpec = { 0.0, 32.0, 0.0004,
                            {0.04, 0.06, 0.0, "%F.S.", "0.04%+0.06%F.S."}, "CCL" };
    rangeLow.voltageSpec = makeVoltRange1200("CCL");
    rangeLow.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.0004, 1.2, 0.0004, 0.0,
                            0.0, 32.0, 0.0004, 0.002, "Slew: 0.4mA/us~1.2A/us" };
    rangeLow.remark = "4800W/CCL";

    PowerRangeSpec63200A rangeMid;
    rangeMid.mode  = "CCM";
    rangeMid.power = 8000.0;
    rangeMid.currentSpec = { 0.0, 160.0, 0.002,
                            {0.04, 0.06, 0.0, "%F.S.", "0.04%+0.06%F.S."}, "CCM" };
    rangeMid.voltageSpec = makeVoltRange1200("CCM");
    rangeMid.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                            0.002, 6.0, 0.002, 0.0,
                            0.0, 160.0, 0.002, 0.002, "Slew: 2mA/us~6A/us" };
    rangeMid.remark = "8000W/CCM";

    PowerRangeSpec63200A rangeHigh;
    rangeHigh.mode  = "CCH";
    rangeHigh.power = 8000.0;
    rangeHigh.currentSpec = { 0.0, 320.0, 0.004,
                             {0.04, 0.06, 0.0, "%F.S.", "0.04%+0.06%F.S."}, "CCH" };
    rangeHigh.voltageSpec = makeVoltRange1200("CCH");
    rangeHigh.dynamicSpec = { 0.00001, 99.999999, 0.000001,
                             0.004, 12.0, 0.004, 0.0,
                             0.0, 320.0, 0.004, 0.002, "Slew: 4mA/us~12A/us" };
    rangeHigh.remark = "8000W/CCH";

    spec.ranges = { rangeLow, rangeMid, rangeHigh };
    return spec;
}
