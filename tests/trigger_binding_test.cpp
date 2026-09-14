#include "triggerbinding.h"
#include "oscilloscope.h"
#include <QCoreApplication>
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

class StubScope : public Oscilloscope {
public:
    QString model() const override { return "Test"; }
    QString vendor() const override { return "Test"; }
    QByteArray captureScreenshot(const QString&, const QString&) override { return {}; }
};

class StubController : public ITriggerController {
public:
    explicit StubController(QString family) : m_family(std::move(family)) {}
    void setInstrument(Oscilloscope* scope) override { m_scope = scope; }
    Oscilloscope* getInstrument() const override { return m_scope; }
    QString getSupportedModel() const override { return m_family; }
    int getSelectedChannel() const override { return 3; }
    void requestReconnect() { emit reconnectRequested(); }
private:
    QString m_family;
    Oscilloscope* m_scope = nullptr;
};

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try {
        StubScope scope;
        StubController first("MSOSeries456"), second("DPO4000");
        TriggerBinding binding;
        int reconnects = 0;
        QObject::connect(&binding, &TriggerBinding::reconnectRequested,
                         &app, [&] { ++reconnects; });
        require(binding.attach(&first, "MSO44B"), "matching family did not attach");
        binding.bindInstrument(&scope);
        require(first.getInstrument() == &scope && binding.selectedChannel() == 3,
                "binding did not reach controller");
        first.requestReconnect();
        require(reconnects == 1, "reconnect not forwarded");
        require(binding.attach(&second, "MSO4000"), "legacy family alias failed");
        require(first.getInstrument() == nullptr, "replaced controller still references instrument");
        first.requestReconnect();
        require(reconnects == 1, "old controller still forwards reconnects");
        binding.bindInstrument(&scope);
        second.requestReconnect();
        require(reconnects == 2, "new controller reconnect lost");
        require(binding.attach(&second, "DPO4000"), "reattach failed");
        second.requestReconnect();
        require(reconnects == 3, "reattach duplicated reconnect subscription");
        require(!binding.attach(&first, "DPO7000") && !binding.hasController(),
                "mismatched family remained attached");
        auto* transient = new StubController("DPO7000");
        require(binding.attach(transient, "DPO7000"), "transient attach failed");
        delete transient;
        require(!binding.hasController() && binding.selectedChannel() == 0,
                "destroyed controller was not cleared");
        binding.bindInstrument(&scope); // must be harmless after controller destruction
        binding.detach();
        require(binding.modelName().isEmpty(), "detach retained model name");
        require(!binding.attach(nullptr, "DPO7000"), "null controller accepted");
        std::cout << "PASS: replacement, reattachment, stale reconnect and controller lifetime without widgets\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
