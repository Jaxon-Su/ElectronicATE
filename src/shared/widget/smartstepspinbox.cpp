#include "SmartStepSpinBox.h"
#include <QString>

SmartStepSpinBox::SmartStepSpinBox(QWidget* parent)
    : QDoubleSpinBox(parent)
{
    setDecimals(2);  // 最多顯示到 2 位小數
}

void SmartStepSpinBox::stepBy(int steps)
{
    double val = value();
    QString text = textFromValue(val);  // 拿目前顯示字串
    int decimals = 0;
    if (text.contains('.')) {
        auto frac = text.section('.', 1, 1);
        // 計算去尾0後剩幾位
        while (frac.endsWith('0') && frac.length() > 1) frac.chop(1);
        decimals = frac.length();
        if (frac == "0") decimals = 0;
    }
    double step = 1.0;
    if (decimals >= 2)
        step = 0.01;
    else if (decimals == 1)
        step = 0.01; //刻意
    else
        step = 1.0;

    setValue(val + step * steps);
}
