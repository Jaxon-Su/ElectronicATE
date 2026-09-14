#include "oscilloscopemanager.h"
#include "oscilloscope.h"
#include <QCoreApplication>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

class LifecycleScope : public Oscilloscope {
public:
    int disconnects = 0;
    bool connected = true;
    bool failStatus = false;
    bool failDisconnect = false;
    QString model() const override { return "OfflineScope"; }
    QString vendor() const override { return "Test"; }
    bool isConnected() const override { if (failStatus) throw 42; return connected; }
    void disconnect() override {
        ++disconnects;
        if (failDisconnect) throw std::runtime_error("disconnect failed");
        connected = false;
    }
    QByteArray captureScreenshot(const QString&, const QString&) override { return {}; }
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        auto first = std::make_shared<LifecycleScope>();
        auto second = std::make_shared<LifecycleScope>();
        OscilloscopeManager manager;
        manager.assign({{"A", first}, {"B", second}});
        require(manager.current() == first && manager.currentModel() == "A", "default selection failed");
        manager.setCurrent("B");
        require(manager.current() == second && manager.currentModel() == "B", "selection failed");
        manager.setCurrent("missing");
        require(manager.current() == second && !manager.get("missing"), "unknown selection changed state");
        manager.clear();
        require(manager.isEmpty() && !manager.current() && manager.currentModel().isEmpty(),
                "clear retained selection");
        require(first->disconnects == 1 && second->disconnects == 1, "clear did not disconnect once");
        OscilloscopeManager::OscMap map{{"A", first}, {"null", nullptr}};
        OscilloscopeManager::disconnectAll(map);
        require(map.isEmpty() && first->disconnects == 1, "cleanup repeated disconnect or retained map");
        manager.assign({});
        require(manager.isEmpty() && manager.modelNames().isEmpty(), "empty assignment failed");
        auto brokenStatus = std::make_shared<LifecycleScope>();
        brokenStatus->failStatus = true;
        auto brokenDisconnect = std::make_shared<LifecycleScope>();
        brokenDisconnect->failDisconnect = true;
        auto healthy = std::make_shared<LifecycleScope>();
        manager.assign({{"A", brokenStatus}, {"B", brokenDisconnect}, {"C", healthy}});
        manager.clear();
        require(manager.isEmpty() && !manager.current() && healthy->disconnects == 1
                    && brokenDisconnect->disconnects == 1,
                "one cleanup exception prevented releasing other scopes");
        std::cout << "PASS: oscilloscope lifecycle without widgets or hardware factories\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
