#include "controlpagelock.h"
#include <QApplication>
#include <stdexcept>
#include <iostream>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    try {
        QTabWidget tabs;
        for (int i = 0; i < 5; ++i) tabs.addTab(new QWidget, QString::number(i));
        for (int page : {0, 1, 2}) {
            tabs.setCurrentIndex(page);
            for (int edit = 0; edit < 3; ++edit) {
                applyControlPageLock(&tabs, {}, true);
                if (tabs.currentIndex() != page || !tabs.isTabEnabled(0) || !tabs.isTabEnabled(1))
                    throw std::runtime_error("background config switched or locked settings page");
                if (tabs.isTabEnabled(3) || tabs.isTabEnabled(4) || tabs.widget(2)->isEnabled())
                    throw std::runtime_error("background connection allowed competing controls");
                applyControlPageLock(&tabs, {}, false);
                if (tabs.currentIndex() != page || !tabs.widget(2)->isEnabled())
                    throw std::runtime_error("config completion did not restore controls in place");
            }
        }
        for (int owner = 0; owner < 3; ++owner) {
            std::array<bool, 3> active{};
            active[owner] = true;
            applyControlPageLock(&tabs, active);
            for (int i = 2; i < 5; ++i)
                if (tabs.isTabEnabled(i) != (i == owner + 2)) throw std::runtime_error("control page lock mismatch");
            if (tabs.isTabEnabled(0) || tabs.isTabEnabled(1)) throw std::runtime_error("configuration changed during hardware ownership");
        }
        applyControlPageLock(&tabs, {true, false, true});
        applyControlPageLock(&tabs, {true, false, false});
        if (tabs.isTabEnabled(3) || tabs.isTabEnabled(4)) throw std::runtime_error("one completion unlocked another active owner");
        applyControlPageLock(&tabs, {});
        for (int i = 0; i < 5; ++i)
            if (!tabs.isTabEnabled(i)) throw std::runtime_error("idle tabs remained disabled");
        std::cout << "PASS: control page enable states\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
