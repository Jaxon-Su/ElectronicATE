#pragma once

#include <QObject>
#include <QVector>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include "page1config.h"
#include "page2config.h"
#include "dutrowdata.h"

class Page5Model : public QObject {
    Q_OBJECT
public:
    explicit Page5Model(QObject* parent = nullptr);

    void writeXml(QXmlStreamWriter& writer) const;
    void loadXml(QXmlStreamReader& reader);

    // ===== 來自 Page1 =====
    Page1Config page1Config;

    // ===== 來自 Page2（fallback 路徑：條件資料提供者為 null 時使用）=====
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
