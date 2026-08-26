#pragma once
#include <QTableWidget>
#include <QLineEdit>
#include "sidepanel.h"

class Page5ViewModel;

// ══════════════════════════════════════════════════════
//  Page5RightPanel — 右側 Conditions 面板
//
//  職責：
//  - 顯示 File / Input / Load / Dynamic / Relay 條件表格
//  - 所有表格唯讀，資料由 ViewModel 提供
//  - refresh slots 由 Page5 透過 ViewModel 信號觸發
// ══════════════════════════════════════════════════════
class Page5RightPanel : public SidePanel
{
    Q_OBJECT

public:
    explicit Page5RightPanel(Page5ViewModel* viewModel, QWidget* parent = nullptr);
    ~Page5RightPanel() override = default;

public slots:
    void refreshInputTable();
    void refreshLoadTable();
    void refreshDynamicTable();
    void refreshRelayTable();

private:
    void buildPanel();

    // 建立各表格（初始空結構）
    QTableWidget* buildFileTable();
    QTableWidget* buildInputTableWidget();
    QTableWidget* buildLoadTableWidget();
    QTableWidget* buildDynamicTableWidget();
    QTableWidget* buildRelayTableWidget();

    // 共用唯讀表格建立輔助
    QTableWidget* makeReadOnlyTable(const QStringList& headers,
                                    int minH, int maxH);

    Page5ViewModel* m_viewModel      = nullptr;

    QTableWidget*   m_fileTable      = nullptr;
    QTableWidget*   m_inputTable     = nullptr;
    QTableWidget*   m_loadTable      = nullptr;
    QTableWidget*   m_dynamicTable   = nullptr;
    QTableWidget*   m_relayTable     = nullptr;
};
