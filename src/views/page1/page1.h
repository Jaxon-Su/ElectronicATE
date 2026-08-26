#pragma once

#include <QWidget>
#include <QMap>
#include <QString>
#include "page1viewmodel.h"

class QTableWidget;
class QSpinBox;
class QCheckBox;
class QGroupBox;
class QVBoxLayout;
class QComboBox;
class Page1ViewModel;
class QLabel;
class QPushButton;

class Page1 : public QWidget {
    Q_OBJECT
public:
    explicit Page1(Page1ViewModel* viewModel, QWidget *parent = nullptr);
    ~Page1();

    void syncUIToViewModel();

signals:
    void uiConfigChanged(const QList<InstrumentConfig>& configs, int loadOutputs, int relayOutputs);

private slots:
    void onInstrumentToggled(int state);
    void resetUIFromViewModel();
    void onConfigButtonClicked();  // 新增：配置按鈕點擊處理

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    // ==================== UI 元件 ====================
    QTableWidget *tableWidget          {nullptr};
    QSpinBox     *spinBox_Load_Outputs {nullptr};
    QSpinBox     *spinBox_Relay_Outputs{nullptr};
    QGroupBox    *groupBox             {nullptr};
    QVBoxLayout  *checkboxLayout       {nullptr};
    QMap<QString, QCheckBox*> instrumentCheckboxes;
    QLabel *label1;
    QLabel *label2;
    QWidget *loadSpinWidget;
    QWidget *relaySpinWidget;
    QWidget *leftContainer;

    // ==================== 資料成員 ====================
    Page1ViewModel *viewModel {nullptr};

    struct RowWidgets {
        QComboBox *modelCb = nullptr;
        QComboBox *addrCb = nullptr;
        QPushButton *configBtn = nullptr;  // 新增：配置按鈕
        QList<QComboBox*> subModelCbs;
        QList<QComboBox*> indexCbs;
        QList<QComboBox*> syncRoleCbs;
    };
    QMap<QString, RowWidgets> m_instrumentWidgets;
    QMap<QString, InstrumentConfig> m_configMap;
    QMap<QString, QString> m_instrumentTypeCache;

    // 新增：儲存每個儀器的通訊配置
    QMap<QString, CommunicationConfig> m_commConfigMap;

    // ==================== 初始化函式 ====================
    void initializeUI();
    void setupLayout();
    void setupCheckboxes(const QMap<QString, bool> &enabledMap = QMap<QString, bool>());
    void setupConnections();

    // ==================== 表格設置主流程 ====================
    void setupTable();
    void prepareTableStructure();
    void buildConfigMap();
    int  setupInstrumentRow(int startRow, const TableRowInfo &rowInfo);
    void finalizeTable();

    // ==================== 列設置函式 ====================
    void setupNameColumn(int row, const QString &name);
    void setupBasicColumns(int row, const TableRowInfo &rowInfo, const InstrumentConfig &ic);
    int  setupChannelRow(int startRow, const TableRowInfo &rowInfo, const InstrumentConfig &ic);
    int  setupSimpleRow(int startRow);

    // ==================== Widget 創建函式 ====================
    QComboBox* createModelComboBox(const QStringList &candidates, const QString &saved);
    QComboBox* createAddressComboBox(const InstrumentConfig &ic);  // 修改：傳入完整配置
    QPushButton* createConfigButton(const QString &instName);      // 新增：創建配置按鈕
    QComboBox* createSubModelComboBox(const InstrumentConfig &ic, int channelIdx,
                                      bool enabled, const QStringList &validSubModels);
    QComboBox* createIndexComboBox(const InstrumentConfig &ic, int channelIdx,
                                   const QString &type, bool enabled);
    QComboBox* createSyncRoleComboBox(const InstrumentConfig &ic, int channelIdx,
                                      const QString &type, bool enabled);

    // ==================== 事件處理 ====================
    void connectModelChangeHandler(int row, const TableRowInfo &rowInfo, QComboBox *modelCb);
    void updateSubModelComboBox(int row, int col, bool enable, const QStringList &subList);
    void updateIndexComboBox(int row, int col, bool enable, const QString &type);
    void updateSyncRoleComboBox(int row, int col, bool enable, const QString &type);

    // 新增：開啟通訊配置對話框
    void openCommConfigDialog(const QString &instName);

    // ==================== Index 管理 ====================
    void enforceUniqueIndices();
    void refreshIndexChoices();

    // ==================== Index 管理輔助函式 ====================
    QPair<QSet<QString>, QSet<QString>> collectUsedIndices() const;
    void updateAllComboBoxOptions(const QSet<QString> &loadUsed, const QSet<QString> &relayUsed);
    void updateComboBoxModel(QComboBox *cb, const QSet<QString> &usedSet) const;

    QString getInstrumentType(int nameRow) const;
    bool isItemAvailable(const QString &itemText, const QString &currentText,
                         const QSet<QString> &usedSet) const;

    // ==================== UI 同步輔助函式 ====================
    QList<InstrumentConfig> collectAllInstrumentConfigs() const;
    InstrumentConfig collectSingleInstrumentConfig(int &row, const QString &instName,
                                                   const TableRowInfo &info) const;
    QList<ChannelSetting> collectChannelSettings(int topRow) const;

    // ==================== Outputs 更新 ====================
    void applyLoadOutputs(int newVal);
    void applyRelayOutputs(int newVal);
    void applyOutputsChange(const QString &type, int newVal);

    // ==================== 樣式與工具 ====================
    void applyComboBoxStyle(QComboBox *comboBox, bool editable = false);
    QString getComboBoxStyle() const;
    void autoSyncOnWidgetChanged(QWidget* widget);
    void updateInstrumentVisibility(const QString &inst, bool visible);
};
