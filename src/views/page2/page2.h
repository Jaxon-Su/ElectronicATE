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
    void relayRowsChanged(const QVector<RelayDataRow>&);
    void loadMetaChanged(const LoadMetaRow&);
    void loadRowsChanged(const QVector<LoadDataRow>&);
    void dynamicMetaChanged(const DynamicMetaRow&);
    void dynamicRowsChanged(const QVector<DynamicDataRow>&);

private slots:
    void onHeadersChanged(LoadKind kind, const QStringList &headers);
    void onRowAddRequested(LoadKind kind, const QStringList &validatorTags);
    void onRowRemoveRequested(LoadKind kind);
    void onInputTitleChanged(int row, const QString &dummy);
    void onMaxOutputChanged(int maxOut);
    void onPowerUpdated(int row, double value);
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
    QTableWidget* tableByKind(LoadKind k) const;
    LoadKind kindOf(const QTableWidget *tbl) const;

    // Meta 行建立
    void ensureRelayMetaRows(int maxOutput);
    void ensureLoadMetaRows(int maxOutput);
    void ensureDynamicMetaRows(int maxOutput);
    void ensurePowerColumn(int maxOutput);
    void createMetaHeaderLabel(QTableWidget* tbl, int row, const QString& text);

    // 行擴展與創建
    void extendRows(LoadKind kind, const QStringList &tags);
    void createRowWidgets(QTableWidget *tbl, const QStringList &tags);

    // Widget 工廠方法
    static QValidator* makeDoubleVal(QObject* p, int dec = 3);
    static QValidator* makeRangeVal(QObject* p);
    static QLineEdit* makeLineEdit(QTableWidget* tbl, int r, int c, QChar tag,
                                   Page2* self, Page2ViewModel* vm, LoadKind kind);
    static QComboBox* makeComboBox(QTableWidget* tbl, int r, int c,
                                   Page2* self, Page2ViewModel* vm, LoadKind kind);

    // 驗證器標籤輔助函數
    QChar determineValidatorTag(QTableWidget* tbl, int row, int col, const QStringList& tags) const;
    bool shouldSkipMetaRow(QTableWidget* tbl, int row) const;

    // 鍵盤導航輔助函數
    QTableWidget* findTableForWidget(QWidget* widget) const;
    bool handleNavigationKey(QKeyEvent* keyEvent, QTableWidget* tbl, int row, int col);

    // 同步輔助函數
    void syncInputTable();
    void syncRelayTable();
    void syncLoadTable();
    void syncDynamicTable();
    QVector<QString> extractMetaRowValues(QTableWidget* tbl, int row, int maxOutput);

    // 重置輔助函數
    void resetInputTable();
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
    void fillLoadMetaCell(int row, int col, const QVector<QString>& values);
    void fillLoadDataRows(int maxOutput, int metaRows, int dataRows);
    void fillLoadDataLabel(int row, const QString& label);
    void fillLoadDataValues(int row, int maxOutput, const QVector<QString>& values);
    void fillLoadPowerCell(int row, int maxOutput, int dataRowIndex);

    // Dynamic 表格重置細分函數
    void setupDynamicTableStructure(int dMaxOutput, int dDataRows);
    void fillDynamicMetaRows(int dMaxOutput);
    void fillDynamicMetaCell(int row, int col, const QVector<QString>& values);
    void fillDynamicDataRows(int dMaxOutput, int dDataRows);
    void fillDynamicDataLabel(int row, const QString& label);
    void fillDynamicDataValues(int row, int dMaxOutput, const QVector<QString>& values);

    // 表格標題處理
    void setupTableHeaders(LoadKind kind, const QStringList &headers);
    void handleRelayHeaders(int maxOutput);
    void handleLoadHeaders(int maxOutput);
    void handleDynamicHeaders(int maxOutput);

    // 連接輔助函數
    void connectTableItemChanged(QTableWidget* tbl, LoadKind kind, int metaRows);
    void connectButtonToViewModel(QPushButton* btn, LoadKind kind, bool isAdd);

    // Dynamic T1~T2表格特殊處理
    void handleDynamicHeadersTime(const QStringList &headers);
    void ensureT1T2ColumnSetup(int t1t2Col);

private:
    // UI 元件
    QTableWidget *tblInput      = nullptr;
    QTableWidget *tblRelay      = nullptr;
    QTableWidget *tblLoad       = nullptr;
    QTableWidget *tblDynamic    = nullptr;

    QPushButton  *btnAddInput   = nullptr;
    QPushButton  *btnSubInput   = nullptr;
    QPushButton  *btnAddRelay   = nullptr;
    QPushButton  *btnSubRelay   = nullptr;
    QPushButton  *btnAddLoad    = nullptr;
    QPushButton  *btnSubLoad    = nullptr;
    QPushButton  *btnAddDynamic = nullptr;
    QPushButton  *btnSubDynamic = nullptr;

    Page2ViewModel *vm = nullptr;

    // 常量
    static constexpr int kMetaRowsLoad    = 8;
    static constexpr int kMetaRowsDynamic = 6;
    static constexpr int kMetaRowsRelay   = 0;
};
