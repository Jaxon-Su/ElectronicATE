#include "oscilloscopebuilder.h"
#include "oscilloscope.h"
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

class BuildScope : public Oscilloscope {
public:
    QString model() const override { return "Offline"; }
    QString vendor() const override { return "Test"; }
    QByteArray captureScreenshot(const QString&, const QString&) override { return {}; }
    void connect() override { ++attempts; if (fail) throw std::runtime_error("connect failed"); }
    bool isConnected() const override { return connected; }
    bool connected = true;
    bool fail = false;
    int attempts = 0;
};

InstrumentConfig instrument(const QString& model)
{
    InstrumentConfig value;
    value.type = "Oscilloscope";
    value.modelName = model;
    value.address = "legacy-resource";
    return value;
}

int main()
{
    try {
        Page1Config config;
        auto disabled = instrument("disabled"); disabled.enabled = false;
        auto other = instrument("other"); other.type = "Load";
        auto missing = instrument("missing"); missing.address.clear();
        auto typed = instrument("typed");
        typed.address.clear();
        typed.commConfig.protocol = ProtocolType::GPIB;
        typed.commConfig.gpibAddress = 8;
        auto preferred = typed;
        preferred.modelName = "preferred";
        preferred.address = "obsolete-resource";
        config.instruments = {disabled, other, missing, instrument(""), instrument("legacy"), typed,
                              preferred, instrument("null"), instrument("disconnected"),
                              instrument("throws"), instrument("factory-throws"), instrument("last")};
        QStringList called;
        QMap<QString, std::shared_ptr<BuildScope>> scopes;
        const auto built = buildConnectedOscilloscopes(config,
            [&](const QString& model, const QString& resource) -> std::shared_ptr<Oscilloscope> {
                called << model;
                if (model == "typed" || model == "preferred")
                    require(resource == "GPIB0::8::INSTR", "typed resource was ignored");
                else require(resource == "legacy-resource", "legacy resource changed");
                if (model == "null") return {};
                if (model == "factory-throws") throw 42;
                auto scope = std::make_shared<BuildScope>();
                scope->connected = model != "disconnected";
                scope->fail = model == "throws";
                scopes[model] = scope;
                return scope;
            });
        require(called.size() == 8 && !called.contains("disabled") && !called.contains("missing"),
                "invalid/disabled settings reached factory");
        require(built.keys() == QStringList{"last", "legacy", "preferred", "typed"},
                "failed connections retained or later valid connection skipped");
        for (const auto& scope : scopes) require(scope->attempts == 1, "connection attempted more than once");
        require(buildConnectedOscilloscopes(config, {}).isEmpty(), "absent creator should produce empty map");
        std::cout << "PASS: factory-independent connection filtering, resource selection and failure isolation\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
