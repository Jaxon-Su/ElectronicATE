#pragma once
#include <QWidget>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QVector>
#include <QVBoxLayout>
#include "page5taskpayload.h"   // RunTask

// ══════════════════════════════════════════════════════
//  Page5RunPanel — Test Task 執行面板
//
//  職責：
//   ① 接收 Active RunTask 清單（含穩定 dutUid）
//   ② 顯示每列執行狀態（Idle / Running / Pass / Fail）
//   ③ 發出 runRequested(QVector<RunTask>) / stopRequested
//
//  狀態流程：
//   Idle ──[Run]──► Running ──[完成]──► Pass / Fail
//                       └──[Stop]──► Idle（reset）
// ══════════════════════════════════════════════════════
class Page5RunPanel : public QWidget
{
    Q_OBJECT

public:
    enum class TaskStatus { Idle, Running, Pass, Fail };

    explicit Page5RunPanel(QWidget* parent = nullptr);
    ~Page5RunPanel() override = default;

    // 由 Page5CenterPanel 呼叫（含穩定 dutUid）
    void setActiveTasks(const QVector<RunTask>& tasks);

    // 由外部執行層呼叫（index = RunPanel 內的順序）
    void setTaskStatus(int index, TaskStatus status);
    void setRetryCount(int index, int count);

    // 全部重置為 Idle（含控制按鈕狀態）
    void resetAll();

    // 僅重置 task 列狀態為 Idle，不改變按鈕狀態
    void resetTaskRows();

    // 供 Worker / Page5 查詢：RunPanel row i 的 uid
    int dutUidAt(int row) const
    {
        if (row < 0 || row >= m_tasks.size()) return -1;
        return m_tasks[row].dutUid;
    }

signals:
    void runRequested(const QVector<RunTask>& tasks);   // 含 dutUid
    void stopRequested();

private:
    void buildUi();
    void buildToolBar(QWidget* parent, QVBoxLayout* layout);
    void buildTaskTable(QWidget* parent, QVBoxLayout* layout);
    void updateControlState(bool running);
    QString statusText(TaskStatus s) const;
    QString statusStyle(TaskStatus s) const;

    QTableWidget*    m_table     = nullptr;
    QPushButton*     m_runBtn    = nullptr;
    QPushButton*     m_stopBtn   = nullptr;
    QLabel*          m_statusLbl = nullptr;

    QVector<RunTask> m_tasks;           // 含 dutUid
    bool             m_running = false;
};

Q_DECLARE_METATYPE(Page5RunPanel::TaskStatus)
