#pragma once
#include <QString>
#include <QVector>

struct AccuracySpec {
    double percentOfReading = 0.0;
    double percentOfFS = 0.0;
    double fixedError = 0.0;
    QString fixedUnit;
    QString remark;
};

struct CurrentRangeSpec {
    double minCurrent = 0.0;
    double maxCurrent = 0.0;
    double resolution = 0.0;
    AccuracySpec accuracy;
    QString remark;
};

struct VoltageRangeSpec {
    double minVoltage = 0.0;
    double maxVoltage = 0.0;
    double resolution = 0.0;
    AccuracySpec accuracy;
    QString remark;
};

struct ResistanceRangeAccuracy {
    double minOhm = 0.0;
    double maxOhm = 0.0;
    AccuracySpec accuracy;
    QString remark;
};

struct ResistanceSpec {
    QVector<ResistanceRangeAccuracy> rangeAccuracyList;
    int resolutionBits = 0;
    QString remark;
};

struct DynamicSpec {
    double t1Min = 0.0;       // T1/T2 最小值 (s)
    double t1Max = 0.0;       // T1/T2 最大值 (s)
    double t1Resolution = 0.0;// T1/T2 解析度 (s)
    double slewMin = 0.0;     // Slew Rate 最小 (A/us)
    double slewMax = 0.0;     // Slew Rate 最大 (A/us)
    double slewResolution = 0.0; // Slew Rate 解析度 (A/us)
    double accuracy = 0.0;    // 動態精度（如1us/100ppm等，視實際需求）
    double currentMin = 0.0;  // 動態電流範圍
    double currentMax = 0.0;
    double currentResolution = 0.0;
    double currentAccuracy = 0.0; // 通常以F.S.百分比
    QString remark;           // 其它描述
};

struct PowerRangeSpec {
    double power = 0.0; // ex:63101 20W, 200W
    CurrentRangeSpec currentSpec;
    VoltageRangeSpec voltageSpec;
    ResistanceSpec resistanceSpec;
    DynamicSpec dynamicSpec;
    QString remark;
};

struct ChromaLoadSpec {
    QString model;
    QVector<PowerRangeSpec> ranges;
};

ChromaLoadSpec createChroma63101Spec();
ChromaLoadSpec createChroma63102Spec();
ChromaLoadSpec createChroma63103Spec();
ChromaLoadSpec createChroma63105Spec();
ChromaLoadSpec createChroma63106Spec();
ChromaLoadSpec createChroma63108Spec();
ChromaLoadSpec createChroma63112Spec();

std::optional<PowerRangeSpec> findPowerRange(const QString& subModel, double currval);

QString selectOptimalLoadMode(const QString& subModel,
                              double current,
                              double voltage);
