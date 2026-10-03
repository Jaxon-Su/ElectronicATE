#pragma once

#include <QObject>
#include <QVector>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include "page2config.h"

class Page2Model : public QObject {
    Q_OBJECT
  public:
    explicit Page2Model(QObject* parent = nullptr);

    void writeXml(QXmlStreamWriter& writer) const;
    void loadXml(QXmlStreamReader& reader);

    const DynamicMetaRow& getDynamicMeta() const { return dynamicMeta; }
    void setDynamicMeta(const DynamicMetaRow& value) { dynamicMeta = value; }
    const LoadMetaRow& getLoadMeta() const { return loadMeta; }
    void setLoadMeta(const LoadMetaRow& value) { loadMeta = value; }
    const QVector<DcRow>& getDcRows() const { return dcRows; }
    const QVector<DcRow>& dcSourceRows(int source) const
    {
        return source == 0 ? dcRows : source == 1 ? dcRows2 : dcRows3;
    }
    const QVector<QString>& getDcNames() const { return dcNames; }
    void setDcRows(const QVector<DcRow>& value) { dcRows = value; }
    const QVector<RelayDataRow>& getRelayRows() const { return relayRows; }
    void setRelayRows(const QVector<RelayDataRow>& value) { relayRows = value; }
    const QVector<InputRow>& getInputRows() const { return inputRows; }
    void setInputRows(const QVector<InputRow>& value) { inputRows = value; }
    const QVector<LoadDataRow>& getLoadRows() const { return loadRows; }
    void setLoadRows(const QVector<LoadDataRow>& value) { loadRows = value; }
    const QVector<DynamicDataRow>& getDynamicRows() const { return dynamicRows; }
    void setDynamicRows(const QVector<DynamicDataRow>& value) { dynamicRows = value; }
    void resizeLoadOutputs(int count);
    void resizeRelayOutputs(int count);
    TestConditionSnapshot snapshot() const
    {
        return {inputRows,   dcRows,      relayRows, loadMeta, loadRows,
                dynamicMeta, dynamicRows, dcRows2,   dcRows3,  dcNames};
    }
    void setSnapshot(const TestConditionSnapshot& value)
    {
        inputRows = value.inputRows;
        dcRows = value.dcRows;
        dcRows2 = value.dcRows2;
        dcRows3 = value.dcRows3;
        dcNames = value.dcNames;
        relayRows = value.relayRows;
        loadMeta = value.loadMeta;
        loadRows = value.loadRows;
        dynamicMeta = value.dynamicMeta;
        dynamicRows = value.dynamicRows;
    }

  private:
    QVector<DcRow> dcRows2, dcRows3;
    QVector<QString> dcNames;
    QVector<InputRow> inputRows;
    QVector<DcRow> dcRows;
    QVector<RelayDataRow> relayRows;
    LoadMetaRow loadMeta;
    QVector<LoadDataRow> loadRows;
    DynamicMetaRow dynamicMeta;
    QVector<DynamicDataRow> dynamicRows;

  signals:
    void configLoaded();

  private:
    // XML 寫入輔助
    class XmlWriter {
      public:
        static void writeInputTable(QXmlStreamWriter& w, const QVector<InputRow>& rows);
        static void writeRelayTable(QXmlStreamWriter& w, const QVector<RelayDataRow>& rows);
        static void writeLoadTable(QXmlStreamWriter& w, const LoadMetaRow& meta,
                                   const QVector<LoadDataRow>& rows);
        static void writeDynamicTable(QXmlStreamWriter& w, const DynamicMetaRow& meta,
                                      const QVector<DynamicDataRow>& rows);

      private:
        static void writeDataRow(QXmlStreamWriter& w, const QString& label, const QVector<QString>& values);
        static void writeStringVector(QXmlStreamWriter& w, const QString& tag, const QVector<QString>& vec);
    };

    // XML 讀取輔助
    class XmlReader {
      public:
        static void readInputTable(QXmlStreamReader& r, QVector<InputRow>& rows);
        static void readRelayTable(QXmlStreamReader& r, QVector<RelayDataRow>& rows);
        static void readLoadTable(QXmlStreamReader& r, LoadMetaRow& meta, QVector<LoadDataRow>& rows);
        static void readDynamicTable(QXmlStreamReader& r, DynamicMetaRow& meta,
                                     QVector<DynamicDataRow>& rows);

      private:
        static InputRow readInputRow(QXmlStreamReader& r);
        static RelayDataRow readRelayDataRow(QXmlStreamReader& r);
        static LoadDataRow readLoadDataRow(QXmlStreamReader& r);
        static DynamicDataRow readDynamicDataRow(QXmlStreamReader& r);

        static void readLoadMeta(QXmlStreamReader& r, LoadMetaRow& meta);
        static void readDynamicMeta(QXmlStreamReader& r, DynamicMetaRow& meta);

        static QVector<QString> readStringVector(QXmlStreamReader& r, const QString& tag);
        static void skipToEndElement(QXmlStreamReader& r, const QString& elementName);
    };
};
