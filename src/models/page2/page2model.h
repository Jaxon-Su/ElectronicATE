#pragma once

#include <QObject>
#include <QVector>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include "page2config.h"

class Page2Model : public QObject {
    Q_OBJECT
public:
    explicit Page2Model(QObject *parent = nullptr);

    void writeXml(QXmlStreamWriter& writer) const;
    void loadXml(QXmlStreamReader& reader);

    QVector<InputRow>        inputRows;
    QVector<RelayDataRow>    relayRows;
    LoadMetaRow              loadMeta;
    QVector<LoadDataRow>     loadRows;
    DynamicMetaRow           dynamicMeta;
    QVector<DynamicDataRow>  dynamicRows;

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
        static void writeDataRow(QXmlStreamWriter& w, const QString& label,
                                 const QVector<QString>& values);
        static void writeStringVector(QXmlStreamWriter& w, const QString& tag,
                                      const QVector<QString>& vec);
    };

    // XML 讀取輔助
    class XmlReader {
    public:
        static void readInputTable(QXmlStreamReader& r, QVector<InputRow>& rows);
        static void readRelayTable(QXmlStreamReader& r, QVector<RelayDataRow>& rows);
        static void readLoadTable(QXmlStreamReader& r, LoadMetaRow& meta,
                                  QVector<LoadDataRow>& rows);
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
