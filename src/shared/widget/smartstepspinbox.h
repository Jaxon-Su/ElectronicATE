#pragma once

#include <QDoubleSpinBox>

// 動態調整 step 的 QDoubleSpinBox
class SmartStepSpinBox : public QDoubleSpinBox
{
    Q_OBJECT
public:
    explicit SmartStepSpinBox(QWidget* parent = nullptr);

protected:
    void stepBy(int steps) override;
};
