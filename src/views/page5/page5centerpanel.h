#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextEdit>
#include <QStringList>
#include <QMap>
#include <QVariant>
#include "page5model.h"
#include "page5runpanel.h"   // 間接引入 page5taskpayload.h → RunTask

class Page5ViewModel;

// ══════════════════════════════════════════════════════
//  Page5CenterPanel — 中間面板
//
//  雙向資料流：
//  ┌─────────────────────────────────────────────────┐
//  │  UI 操作（新增/刪除/貼上）                        │
//  │      → notifyDutRowsChanged()                   │
//  │      → emit dutRowsChanged(rows)                │
//  │      → ViewModel::onDutRowsChanged() 存入 Model │
//  │                                                  │
//  │  XML 載入完成                                     │
//  │      → ViewModel::dutTableChanged signal        │
//  │      → loadDutRows() 從 ViewModel 填表           │
//  └─────────────────────────────────────────────────┘
// ══════════════════════════════════════════════════════
class Page5CenterPanel : public QWidget
{
    Q_OBJECT

public:
    explicit Page5CenterPanel(Page5ViewModel* viewModel, QWidget* parent = nullptr);
    ~Page5CenterPanel() override = default;

    void setTaskList(const QStringList& tasks);
    Page5RunPanel* runPanel() const { return m_runPanel; }
    void switchToRunTab();

    // ★ 供 Worker / Page5 查詢各類設定（key = uid，穩定不變）
    QVariantMap oscSettings    (int uid) const { return m_oscSettings.value(uid);     }
    QVariantMap delaySettings  (int uid) const { return m_delaySettings.value(uid);   }
    QVariantMap turnOnSettings (int uid) const { return m_turnOnSettings.value(uid);  }
    QVariantMap turnOffSettings(int uid) const { return m_turnOffSettings.value(uid); }
    QVariantMap relaySettings  (int uid) const { return m_relaySettings.value(uid);   }
    QVariantMap staticSettings  (int uid) const { return m_staticSettings.value(uid);   }
    QVariantMap dynamicSettings  (int uid) const { return m_dynamicSettings.value(uid);  }
    QVariantMap shortOnSettings  (int uid) const { return m_shortOnSettings.value(uid);  }
    QVariantMap onShortSettings  (int uid) const { return m_onShortSettings.value(uid);  }

    // ★ 供 collectPayloads() 查詢列的 ext / retry / report
    DutRowData  dutRowForUid   (int uid) const;

    // 供 OutputWindow 寫入日誌
    void appendLog(const QString& msg);

public slots:
    void addDutTestRow(const QString& taskName);
    void loadDutRows();
    void syncActiveTasksToRunPanel();   // Active 勾選變更時同步至 RunPanel
    void setLocked(bool locked);        // 執行中 → 鎖定所有編輯操作

signals:
    void dutRowsChanged(const QVector<DutRowData>& rows);   // 通知 ViewModel 儲存

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildUi();
    QWidget*      buildTabWidget();
    QTableWidget* buildDUTTestTable();
    QWidget*      buildOutputSection();

    void notifyDutRowsChanged();
    void refreshDutSeq();
    void copySelectedRows();
    void pasteRows(int insertAfterRow);
    void insertDutRow(int at, const DutRowData& d);

    // ★ UID 輔助
    int         uidOfRow(int row) const;
    void        removeSettingsForUid(int uid);
    QVariantMap collectSettingsForUid(int uid) const;   // 供 notifyDutRowsChanged 使用

    Page5ViewModel*        m_viewModel    = nullptr;
    QTableWidget*          m_dutTestTable = nullptr;
    QTextEdit*             m_outputWindow = nullptr;
    Page5RunPanel*         m_runPanel     = nullptr;
    QTabWidget*            m_tabs         = nullptr;   // 用於 locked 時停用 tab 切換
    QStringList            m_taskList;
    QList<DutRowData>      m_clipboard;

    // ★ guard：loadDutRows / insertDutRow 程式碼填表時設 true
    //   避免 itemChanged / checkbox toggled 在填表期間觸發存入邏輯
    bool m_loadingData = false;

    // ★ UID 產生器（單調遞增，同一執行期內唯一）
    int m_nextUid = 0;

    // ★ 設定 Map key = uid（非 row index，增刪列後穩定不變）
    QMap<int, QVariantMap> m_oscSettings;
    QMap<int, QVariantMap> m_delaySettings;
    QMap<int, QVariantMap> m_turnOnSettings;
    QMap<int, QVariantMap> m_turnOffSettings;
    QMap<int, QVariantMap> m_relaySettings;
    QMap<int, QVariantMap> m_staticSettings;
    QMap<int, QVariantMap> m_dynamicSettings;
    QMap<int, QVariantMap> m_shortOnSettings;   // "Short then turn on"
    QMap<int, QVariantMap> m_onShortSettings;   // "Turn on then short"
};
