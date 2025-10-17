#pragma once

#include <QObject>
#include <QVector>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include "page1config.h"
#include "page2config.h"

class Page3Model : public QObject {
    Q_OBJECT
public:
    explicit Page3Model(QObject* parent = nullptr);

    void setInputTitles(const QStringList& titles) { m_inputTitles = titles; }
    void setLoadTitles(const QStringList& titles) { m_loadTitles = titles; }
    void setdyLoadTitles(const QStringList& titles) { m_dyloadTitles = titles; }
    void setRelayTitles(const QStringList& titles) { m_relayTitles = titles; }

    void setPage1ConfigChanged(const Page1Config &cfg) { m_page1Config = cfg; }
    void setPage2InputDataChanged(const QVector<InputRow>& rows) { m_page2InputData = rows; }

    void setPage2LoadMetaDataChanged(const LoadMetaRow& meta) { m_LoadMetaData = meta; }
    void setPage2LoadRowsChanged(const QVector<LoadDataRow>& rows) { m_LoadRowsData = rows; }
    void setPage2DynamicMetaChanged(const DynamicMetaRow& meta) { m_DynamicMetaData = meta; }
    void setPage2DynamicRowsChanged(const QVector<DynamicDataRow>& rows) { m_DynamicRowsData = rows; }

    void setSelectedInputState(int index, const QString& text) {
        m_selectedInputIndex = index;
        m_selectedInputText = text;
    }
    void setSelectedLoadState(int index, const QString& text) {
        m_selectedLoadIndex = index;
        m_selectedLoadText = text;
    }
    void setSelectedDyLoadState(int index, const QString& text) {
        m_selectedDyLoadIndex = index;
        m_selectedDyLoadText = text;
    }
    void setSelectedRelayState(int index, const QString& text) {
        m_selectedRelayIndex = index;
        m_selectedRelayText = text;
    }

    const QStringList& getRelayTitles() const { return m_relayTitles; }
    const QStringList& getInputTitles() const { return m_inputTitles; }
    const QStringList& getLoadTitles() const { return m_loadTitles; }
    const QStringList& getDyLoadTitles() const { return m_dyloadTitles; }

    const LoadMetaRow& getLoadMetaData() const { return m_LoadMetaData; }
    const QVector<LoadDataRow>& getLoadRowsData() const { return m_LoadRowsData; }
    const DynamicMetaRow& getDynamicMetaData() const { return m_DynamicMetaData; }
    const QVector<DynamicDataRow>& getDynamicRowsData() const { return m_DynamicRowsData; }

    int getSelectedInputIndex() const { return m_selectedInputIndex; }
    QString getSelectedInputText() const { return m_selectedInputText; }
    int getSelectedLoadIndex() const { return m_selectedLoadIndex; }
    QString getSelectedLoadText() const { return m_selectedLoadText; }
    int getSelectedDyLoadIndex() const { return m_selectedDyLoadIndex; }
    QString getSelectedDyLoadText() const { return m_selectedDyLoadText; }
    int getSelectedRelayIndex() const { return m_selectedRelayIndex; }
    QString getSelectedRelayText() const { return m_selectedRelayText; }

    void writeXml(QXmlStreamWriter& writer) const;
    void loadXml(QXmlStreamReader& reader);

private:
    // XML 寫入輔助
    void writeComboBoxTitles(QXmlStreamWriter& w) const;
    void writeCurrentSelections(QXmlStreamWriter& w) const;
    void writeLoadData(QXmlStreamWriter& w) const;
    void writeDynamicData(QXmlStreamWriter& w) const;

    // XML 讀取輔助
    void readComboBoxTitles(QXmlStreamReader& r);
    void readCurrentSelections(QXmlStreamReader& r);
    void readLoadMetaData(QXmlStreamReader& r);
    void readLoadRowsData(QXmlStreamReader& r);
    void readDynamicMetaData(QXmlStreamReader& r);
    void readDynamicRowsData(QXmlStreamReader& r);

    // 通用工具
    static void writeTitleList(QXmlStreamWriter& w, const QString& tag, const QStringList& list);
    static void writeStringList(QXmlStreamWriter& w, const QString& parentTag,
                                const QString& itemTag, const QVector<QString>& list);
    static QStringList readTitleList(QXmlStreamReader& r, const QString& endTag);
    static QVector<QString> readStringList(QXmlStreamReader& r, const QString& endTag,
                                           const QString& itemTag);

private:
    QStringList m_inputTitles;
    QStringList m_loadTitles;
    QStringList m_dyloadTitles;
    QStringList m_relayTitles;

    Page1Config m_page1Config;
    QVector<InputRow> m_page2InputData;

    LoadMetaRow m_LoadMetaData;
    QVector<LoadDataRow> m_LoadRowsData;
    DynamicMetaRow  m_DynamicMetaData;
    QVector<DynamicDataRow> m_DynamicRowsData;

    int m_selectedInputIndex = -1;
    int m_selectedLoadIndex = -1;
    int m_selectedDyLoadIndex = -1;
    int m_selectedRelayIndex = -1;

    QString m_selectedInputText;
    QString m_selectedLoadText;
    QString m_selectedDyLoadText;
    QString m_selectedRelayText;
};
