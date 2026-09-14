#pragma once

#include <QObject>
#include <QVector>
#include <QXmlStreamWriter>
#include <QXmlStreamReader>
#include "page2config.h"

class Page3Model : public QObject {
    Q_OBJECT
public:
    explicit Page3Model(QObject* parent = nullptr);

    void setInputTitles(const QStringList& titles) { m_state.m_inputTitles = titles; }
    void setLoadTitles(const QStringList& titles) { m_state.m_loadTitles = titles; }
    void setDyLoadTitles(const QStringList& titles) { m_state.m_dyloadTitles = titles; }
    void setRelayTitles(const QStringList& titles) { m_state.m_relayTitles = titles; }

    // Load / Dynamic 相關
    void setPage2LoadMetaDataChanged(const LoadMetaRow& meta) { m_state.m_LoadMetaData = meta; }
    void setPage2LoadRowsChanged(const QVector<LoadDataRow>& rows) { m_state.m_LoadRowsData = rows; }
    void setPage2DynamicMetaChanged(const DynamicMetaRow& meta) { m_state.m_DynamicMetaData = meta; }
    void setPage2DynamicRowsChanged(const QVector<DynamicDataRow>& rows) { m_state.m_DynamicRowsData = rows; }

    // Relay 資料存取
    void setPage2RelayRowsChanged(const QVector<RelayDataRow>& rows) { m_state.m_RelayRowsData = rows; }
    QVector<RelayDataRow> getRelayRowsData() const { return m_state.m_RelayRowsData; }

    // Getters
    QStringList getInputTitles() const { return m_state.m_inputTitles; }
    QStringList getLoadTitles() const { return m_state.m_loadTitles; }
    QStringList getDyLoadTitles() const { return m_state.m_dyloadTitles; }
    QStringList getRelayTitles() const { return m_state.m_relayTitles; }

    LoadMetaRow getLoadMetaData() const { return m_state.m_LoadMetaData; }
    QVector<LoadDataRow> getLoadRowsData() const { return m_state.m_LoadRowsData; }
    DynamicMetaRow getDynamicMetaData() const { return m_state.m_DynamicMetaData; }
    QVector<DynamicDataRow> getDynamicRowsData() const { return m_state.m_DynamicRowsData; }

    // UI State Setters
    void setSelectedInputState(int index, const QString& text) {
        m_state.m_selectedInputIndex = index;
        m_state.m_selectedInputText = text;
    }
    void setSelectedLoadState(int index, const QString& text) {
        m_state.m_selectedLoadIndex = index;
        m_state.m_selectedLoadText = text;
    }
    void setSelectedDyLoadState(int index, const QString& text) {
        m_state.m_selectedDyLoadIndex = index;
        m_state.m_selectedDyLoadText = text;
    }
    void setSelectedRelayState(int index, const QString& text) {
        m_state.m_selectedRelayIndex = index;
        m_state.m_selectedRelayText = text;
    }

    // UI State Getters
    int getSelectedInputIndex() const { return m_state.m_selectedInputIndex; }
    QString getSelectedInputText() const { return m_state.m_selectedInputText; }
    int getSelectedLoadIndex() const { return m_state.m_selectedLoadIndex; }
    QString getSelectedLoadText() const { return m_state.m_selectedLoadText; }
    int getSelectedDyLoadIndex() const { return m_state.m_selectedDyLoadIndex; }
    QString getSelectedDyLoadText() const { return m_state.m_selectedDyLoadText; }
    int getSelectedRelayIndex() const { return m_state.m_selectedRelayIndex; }
    QString getSelectedRelayText() const { return m_state.m_selectedRelayText; }

    // XML
    void writeXml(QXmlStreamWriter& writer) const;
    void loadXml(QXmlStreamReader& reader);

private:
    void writeComboBoxTitles(QXmlStreamWriter& w) const;
    void writeCurrentSelections(QXmlStreamWriter& w) const;
    void writeLoadData(QXmlStreamWriter& w) const;
    void writeDynamicData(QXmlStreamWriter& w) const;
    void writeRelayData(QXmlStreamWriter& w) const; // [建議新增]

    void readComboBoxTitles(QXmlStreamReader& r);
    void readCurrentSelections(QXmlStreamReader& r);
    void readLoadMetaData(QXmlStreamReader& r);
    void readLoadRowsData(QXmlStreamReader& r);
    void readDynamicMetaData(QXmlStreamReader& r);
    void readDynamicRowsData(QXmlStreamReader& r);
    void readRelayData(QXmlStreamReader& r); // [建議新增]

    // 通用工具
    static void writeTitleList(QXmlStreamWriter& w, const QString& tag, const QStringList& list);
    static void writeStringList(QXmlStreamWriter& w, const QString& parentTag,
                                const QString& itemTag, const QVector<QString>& list);
    static QStringList readTitleList(QXmlStreamReader& r, const QString& endTag);
    static QVector<QString> readStringList(QXmlStreamReader& r, const QString& endTag,
                                           const QString& itemTag);

private:
    struct State {
    QStringList m_inputTitles;
    QStringList m_loadTitles;
    QStringList m_dyloadTitles;
    QStringList m_relayTitles;

    LoadMetaRow m_LoadMetaData;
    QVector<LoadDataRow> m_LoadRowsData;
    DynamicMetaRow  m_DynamicMetaData;
    QVector<DynamicDataRow> m_DynamicRowsData;

    QVector<RelayDataRow> m_RelayRowsData;

    int m_selectedInputIndex = -1;
    QString m_selectedInputText;
    int m_selectedLoadIndex = -1;
    QString m_selectedLoadText;
    int m_selectedDyLoadIndex = -1;
    QString m_selectedDyLoadText;
    int m_selectedRelayIndex = -1;
    QString m_selectedRelayText;
    };
    State m_state;
    void readXmlFields(QXmlStreamReader& reader);
};
