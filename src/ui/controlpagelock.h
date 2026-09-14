#pragma once
#include <QTabWidget>
#include <array>

// Page3/Page4/Page5 occupy indices 2/3/4. Freeze configuration while any
// controller owns hardware, so OFF cannot be redirected to another instrument.
inline void applyControlPageLock(QTabWidget* tabs, const std::array<bool, 3>& active)
{
    if (!tabs) return;
    for (int index = 0; index < tabs->count(); ++index) {
        bool enabled = !active[0] && !active[1] && !active[2];
        if (index >= 2 && index <= 4) {
            enabled = true;
            for (int other = 0; other < 3; ++other)
                if (other != index - 2 && active[other]) enabled = false;
        }
        tabs->setTabEnabled(index, enabled);
    }
}
