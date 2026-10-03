#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QStringList>
#include <QMap>
#include <QVariant>
#include "dutrowdata.h"
#include "page5runpanel.h" // 間接引入 page5taskpayload.h → RunTask

class Page5ViewModel;

// Renders task drafts; configuration ownership belongs to Page5Model.
class Page5CenterPanel : public QWidget
{
    Q_OBJECT

  public:
    explicit Page5CenterPanel(Page5ViewModel *viewModel, QWidget *parent = nullptr);
    ~Page5CenterPanel() override = default;

    void setTaskList(const QStringList &tasks);
    Page5RunPanel *runPanel() const { return m_runPanel; }
    void switchToRunTab();

    // 供 OutputWindow 寫入日誌
    void appendLog(const QString &msg);
    void setTaskResult(int runIndex, const QString &result);
    void clearResults();

  public slots:
    void addDutTestRow(const QString &taskName);
    void loadDutRows();
    void syncActiveTasksToRunPanel(); // Active 勾選變更時同步至 RunPanel
    void setLocked(bool locked);      // 執行中 → 鎖定所有編輯操作

  signals:
    void dutRowsChanged(const QVector<DutRowData> &rows); // 通知 ViewModel 儲存

  protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

  private:
    void buildUi();
    QWidget *buildTabWidget();
    QTableWidget *buildDUTTestTable();
    QWidget *buildOutputSection();

    void notifyDutRowsChanged();
    void refreshDutSeq();
    void copySelectedRows();
    void pasteRows(int insertAfterRow);
    void insertDutRow(int at, const DutRowData &d);

    // UID 輔助
    int uidOfRow(int row) const;
    void removeSettingsForUid(int uid);
    QVariantMap collectSettingsForUid(int uid) const; // 供 notifyDutRowsChanged 使用

    Page5ViewModel *m_viewModel = nullptr;
    QTableWidget *m_dutTestTable = nullptr;
    QTextEdit *m_outputWindow = nullptr;
    Page5RunPanel *m_runPanel = nullptr;
    QTabWidget *m_tabs = nullptr; // 用於 locked 時停用 tab 切換
    QStringList m_taskList;
    QList<DutRowData> m_clipboard;

    // guard：loadDutRows / insertDutRow 程式碼填表時設 true
    //   避免 itemChanged / checkbox toggled 在填表期間觸發存入邏輯
    bool m_loadingData = false;
};
