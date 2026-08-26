#pragma once
#include <QComboBox>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QStringList>
#include <QEvent>
#include <QMouseEvent>
#include "sidepanel.h"

class Page5LeftPanel : public SidePanel
{
    Q_OBJECT

public:
    explicit Page5LeftPanel(QWidget* parent = nullptr);
    ~Page5LeftPanel() override = default;

    QStringList taskNames() const;

signals:
    void taskDoubleClicked(const QString& taskName);

public slots:
    void setLocked(bool locked);   // 執行中 → 停用雙擊加入任務

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void buildSelectorBar();
    QTreeWidget* buildTree(const QList<QPair<QString, QStringList>>& groups);

    QComboBox*      m_selector = nullptr;
    QStackedWidget* m_stack    = nullptr;
    QTreeWidget*    m_taskTree = nullptr;

    bool            m_locked   = false;   // 執行中禁止雙擊

    struct TaskGroup {
        QString     groupName;
        QStringList tasks;
    };
    static const QList<TaskGroup> TASK_GROUPS;
};
