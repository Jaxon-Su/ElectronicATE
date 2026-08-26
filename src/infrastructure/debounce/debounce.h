#pragma once
#include <QObject>

class QTimer;

// 通用防抖工具：呼叫 schedule() 後延遲 delayMs 才 emit fired()
// 若在計時期間再次呼叫 schedule()，計時重置
class Debounce : public QObject {
    Q_OBJECT
public:
    explicit Debounce(int delayMs, QObject* parent = nullptr);

    void schedule();
    void cancel();
    bool isPending() const;

signals:
    void fired();

private:
    QTimer* m_timer;
    int     m_delayMs;
};
