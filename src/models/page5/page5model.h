#pragma once

#include <QObject>
#include <QVector>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include "page1config.h"
#include "page2config.h"
#include <QJsonDocument>
#include <QJsonObject>

// ══════════════════════════════════════════════════════
//  DutRowData — DUT Test 表格每一列的資料
//  定義在 Model 層，供 Model / ViewModel / CenterPanel 共用
// ══════════════════════════════════════════════════════
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

class Page5Model : public QObject {
    Q_OBJECT
public:
    explicit Page5Model(QObject* parent = nullptr);

    void writeXml(QXmlStreamWriter& writer) const;
    void loadXml(QXmlStreamReader& reader);

    // ===== 來自 Page1 =====
    Page1Config page1Config;

    // ===== 來自 Page2（fallback 路徑：m_page2ViewModel 為 null 時使用）=====
    QVector<InputRow>       inputRows;
    LoadMetaRow             loadMeta;
    QVector<LoadDataRow>    loadRows;
    DynamicMetaRow          dynamicMeta;
    QVector<DynamicDataRow> dynamicRows;
    QVector<RelayDataRow>   relayRows;

    // ===== DUT Test 表格（由 Page5CenterPanel 產生）=====
    QVector<DutRowData> dutRows;

signals:
    void configLoaded();

private:
    void readDutTable(QXmlStreamReader& r);
};
