#pragma once

#include <QObject>
#include <QVector>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include "page1config.h"
#include "page2config.h"
#include "dutrowdata.h"

class Page5Model : public QObject
{
    Q_OBJECT
  public:
    explicit Page5Model(QObject *parent = nullptr);

    const QVector<DutRowData> &tasks() const { return m_tasks; }
    void replaceTasks(QVector<DutRowData> rows);
    int appendTask(DutRowData row);
    QVariantMap taskSettings(int uid) const;
    void setTaskConfig(int uid, const QString &group, const QVariantMap &config);
    void removeTask(int uid);

    void writeXml(QXmlStreamWriter &writer) const;
    void loadXml(QXmlStreamReader &reader);

    // 來自 Page1
    Page1Config page1Config;

    // 來自 Page2（fallback 路徑：條件資料提供者為 null 時使用）
    QVector<InputRow> inputRows;
    LoadMetaRow loadMeta;
    QVector<LoadDataRow> loadRows;
    DynamicMetaRow dynamicMeta;
    QVector<DynamicDataRow> dynamicRows;
    QVector<RelayDataRow> relayRows;

    // DUT Test 表格（由 Page5CenterPanel 產生）
    QString reportName;
    QString reportDirectory;

  signals:
    void configLoaded();

  private:
    QVector<DutRowData> m_tasks;
    int m_nextUid = 0;
    static void readDutTable(QXmlStreamReader &reader, QVector<DutRowData> &rows);
};
