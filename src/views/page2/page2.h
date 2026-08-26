#pragma once
#include <QWidget>
#include "page2viewmodel.h"
#include "page2config.h"
#include <QValidator>
#include <QLineEdit>
#include <QComboBox>

class QTableWidget;
class QPushButton;
class QSpinBox;

class Page2 : public QWidget
{
    Q_OBJECT
public:
    explicit Page2(Page2ViewModel* viewModel, QWidget *parent = nullptr);
    ~Page2() override = default;

    void syncUIToViewModel();

signals:
    void inputRowsChanged(const QVector<InputRow>&);
    void dcRowsChanged(const QVector<DcRow>&);
    void relayRowsChanged(const QVector<RelayDataRow>&);
    void loadMetaChanged(const LoadMetaRow&);
    void loadRowsChanged(const QVector<LoadDataRow>&);
    void dynamicMetaChanged(const DynamicMetaRow&);
    void dynamicRowsChanged(const QVector<DynamicDataRow>&);

private slots:
    void onHeadersChanged(TableKind kind, const QStringList &headers);
    void onRowAddRequested(TableKind kind, const QStringList &validatorTags);
    void onRowRemoveRequested(TableKind kind);
    void onInputTitleChanged(int row, const QString &dummy);
    void onPowerUpdated(int row, double value);
    void onDynamicPowerUpdated(int row, double value);
    void resetUIFromViewModel();

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private:
    // 初始化相關
    void initializeUi();
    void setupLayouts();
    void setupConnections();
    void setupDelegates();
    void setupInitialTableState();

    // 表格工具函數
    QTableWidget* tableByKind(TableKind k) const;
    TableKind kindOf(const QTableWidget *tbl) const;

    // Meta 行建立
    void ensureRelayMetaRows(int maxOutput);
    void ensureLoadMetaRows(int maxOutput);
    void ensureDynamicMetaRows(int maxOutput);
    void ensurePowerColumn(int maxOutput);
    void ensureDynamicPowerColumn(int maxOutput);
    void createMetaHeaderLabel(QTableWidget* tbl, int row, const QString& text);

    // 行擴展與創建
    void extendRows(TableKind kind, const QStringList &tags);
    void createRowWidgets(QTableWidget *tbl, const QStringList &tags);
    void createRowWidgetsAt(QTableWidget *tbl, int row, const QStringList &tags);

    // Widget 工廠方法
    static QValidator* makeDoubleVal(QObject* p, int dec = 3);
    static QValidator* makeRangeVal(QObject* p);
    static QValidator* makeLoadValueVal(QObject* p);
    static QLineEdit* makeLineEdit(QTableWidget* tbl, int r, int c, QChar tag,
                                   Page2* self, Page2ViewModel* vm, TableKind kind);
    static QComboBox* makeComboBox(QTableWidget* tbl, int r, int c,
                                   Page2* self, Page2ViewModel* vm, TableKind kind);
    static QComboBox* makeInputPhaseComboBox(QTableWidget* tbl, int r, int c,
                                             Page2* self, Page2ViewModel* vm);

    // 驗證器標籤輔助函數
    QChar determineValidatorTag(QTableWidget* tbl, int row, int col, const QStringList& tags) const;
    bool shouldSkipMetaRow(QTableWidget* tbl, int row) const;

    // 鍵盤導航輔助函數
    QTableWidget* findTableForWidget(QWidget* widget) const;
    bool handleNavigationKey(QKeyEvent* keyEvent, QTableWidget* tbl, int row, int col);

    // 同步輔助函數
    void syncInputTable();
    void syncDcTable();
    void syncRelayTable();
    void syncLoadTable();
    void syncDynamicTable();
    QVector<QString> extractMetaRowValues(QTableWidget* tbl, int row, int maxOutput);

    // 重置輔助函數
    void resetInputTable();
    void resetDcTable();
    void resetRelayTable();
    void resetLoadTable();
    void resetDynamicTable();

    // Relay 表格重置細分函數
    void setupRelayTableStructure(int maxRelayOutput, int relayDataRows);
    void fillRelayDataRows(int maxRelayOutput, int relayDataRows);
    void fillRelayDataLabel(int row, const QString& label);
    void fillRelayDataComboBoxes(int row, int maxRelayOutput, const QVector<QString>& values);

    // Load 表格重置細分函數
    void setupLoadTableStructure(int maxOutput, int metaRows, int dataRows);
    void fillLoadMetaRows(int maxOutput, int metaRows);
    void fillLoadModeCell(int row, int col, const QVector<QString>& modes);
    void fillLoadRangeCell(int row, int col, const QVector<QString>& ranges);
    void fillLoadMetaCell(int row, int col, const QVector<QString>& values);
    void updateLoadRangeCellOptions(int outputIndex);
    void fillLoadDataRows(int maxOutput, int metaRows, int dataRows);
    void fillLoadDataLabel(int row, const QString& label);
    void fillLoadDataValues(int row, int maxOutput, const QVector<QString>& values);
    void fillLoadPowerCell(int row, int maxOutput, int dataRowIndex);

    // Dynamic 表格重置細分函數
    void setupDynamicTableStructure(int dMaxOutput, int dDataRows);
    void fillDynamicMetaRows(int dMaxOutput);
    void fillDynamicRangeCell(int row, int col, const QVector<QString>& ranges);
    void fillDynamicMetaCell(int row, int col, const QVector<QString>& values);
    void fillDynamicDataRows(int dMaxOutput, int dDataRows);
    void fillDynamicDataLabel(int row, const QString& label);
    void fillDynamicDataValues(int row, int dMaxOutput, const QVector<QString>& values);
    void fillDynamicPowerCell(int row, int dMaxOutput, int dataRowIndex);

    // 表格標題處理
    void setupTableHeaders(TableKind kind, const QStringList &headers);
    void handleRelayHeaders(int maxOutput);
    void handleLoadHeaders();

    // 連接輔助函數
    void connectTableItemChanged(QTableWidget* tbl, TableKind kind, int metaRows);
    void connectButtonToViewModel(QPushButton* btn, TableKind kind, bool isAdd);

    // Dynamic T1~T2 特殊處理
    void handleDynamicHeadersTime(const QStringList &headers);
    void ensureT1T2ColumnSetup(int t1t2Col);

    // Seq 欄 & 右鍵複製/貼上/刪除
    int metaRowsOf(const QTableWidget* tbl) const;
    QStringList tagsForKind(TableKind kind) const;
    void refreshSeqCol(QTableWidget* tbl);

    static QString readCellText(QTableWidget* tbl, int row, int col);
    static void    writeCellText(QTableWidget* tbl, int row, int col, const QString& text);

    void updateTableSelectionVisuals(QTableWidget* tbl);
    void showTableContextMenu(QTableWidget* tbl, const QPoint& viewportPos);
    void copySelectedDataRows(QTableWidget* tbl);
    void pasteDataRows(QTableWidget* tbl, int insertAfterRow);
    void deleteSelectedDataRows(QTableWidget* tbl);

private:
    // ── UI 元件 ──────────────────────────────────────────────────
    QTableWidget *tblInput      = nullptr;
    QTableWidget *tblDc         = nullptr;
    QTableWidget *tblRelay      = nullptr;
    QTableWidget *tblLoad       = nullptr;
    QTableWidget *tblDynamic    = nullptr;

    QPushButton  *btnAddInput   = nullptr;
    QPushButton  *btnSubInput   = nullptr;
    QPushButton  *btnAddDc      = nullptr;
    QPushButton  *btnSubDc      = nullptr;
    QPushButton  *btnAddRelay   = nullptr;
    QPushButton  *btnSubRelay   = nullptr;
    QPushButton  *btnAddLoad    = nullptr;
    QPushButton  *btnSubLoad    = nullptr;
    QPushButton  *btnAddDynamic = nullptr;
    QPushButton  *btnSubDynamic = nullptr;

    Page2ViewModel *vm = nullptr;

    // 常量
    static constexpr int kMetaRowsLoad    = 5;
    static constexpr int kMetaRowsDynamic = 3;
    static constexpr int kMetaRowsRelay   = 0;
    static constexpr int kMetaRowsDc      = 0;

    // 剪貼簿
    struct P2Clipboard {
        TableKind kind = TableKind::Input;
        QVector<QVector<QString>> rows;
    };
    P2Clipboard m_clipboard;
};
