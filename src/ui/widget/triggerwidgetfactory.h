#pragma once
#include <QString>
#include <QWidget>

class TriggerWidgetFactory {
public:
    // modelName: 示波器型號字串（e.g. "DPO7000", "MSO56B"）
    // totalChannels: 僅對 MSO 4/5/6 Series 有效（4 / 6 / 8），其他機型忽略
    static QWidget* createTriggerWidget(const QString& modelName,
                                        QWidget* parent,
                                        QObject** triggerController,
                                        int totalChannels = 4);

private:
    static QWidget* createDPO7000TriggerWidget(QWidget* parent, QObject** controller);
    static QWidget* createDPO4000TriggerWidget(QWidget* parent, QObject** controller);
    static QWidget* createMSOSeries456TriggerWidget(QWidget* parent, QObject** controller,
                                                    int totalChannels);
};
