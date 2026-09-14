#include "outputindexpolicy.h"
#include "channelnumberpolicy.h"
#include "loadcapabilitycatalog.h"
#include "page1config.h"
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

int main()
{
    try {
        InstrumentConfig configured;
        configured.address = "legacy-resource";
        require(configured.getResourceString() == configured.address, "legacy resource fallback lost");
        configured.commConfig.protocol = ProtocolType::GPIB;
        configured.commConfig.gpibAddress = 8;
        require(configured.getResourceString() == "GPIB0::8::INSTR", "structured resource did not override stale address");
        configured.address.clear();
        require(!configured.getResourceString().isEmpty(), "structured-only resource rejected");
        configured.commConfig.protocol = ProtocolType::Modbus_TCP;
        configured.commConfig.ipAddress = "192.168.1.10";
        configured.commConfig.port = 502;
        configured.commConfig.slaveId = 7;
        require(configured.getResourceString().endsWith("::SLAVE:7"), "resolved relay resource lost slave ID");
        QSet<QString> used;
        require(OutputIndexPolicy::claim("1", used) && !OutputIndexPolicy::claim("1", used), "duplicate output accepted");
        require(OutputIndexPolicy::claim("", used) && !used.contains(""), "empty output reserved an index");
        require(OutputIndexPolicy::isAvailable("1", "1", used)
                    && !OutputIndexPolicy::isAvailable("1", "2", used)
                    && OutputIndexPolicy::isAvailable("", "2", used), "output availability changed");
        const auto channels = ChannelNumberPolicy::sorted(ChannelNumberPolicy::parse({"7", "1", "bad", "7", "3"}));
        require(channels == QList<int>{1, 3, 7}, "channel catalog sorting/deduplication changed");
        require(ChannelNumberPolicy::assign(4, channels) == QList<int>{1, 3, 7, -1}, "sparse channel assignment changed");
        require(ChannelNumberPolicy::assign(0, channels).isEmpty(), "empty instrument gained channels");
        require(OutputIndexPolicy::choices(3) == QStringList{"", "1", "2", "3"}, "output choices changed");
        require(OutputIndexPolicy::choices(-1) == QStringList{""}, "negative count offered outputs");
        for (const QString& value : QStringList{"", "0", "-1", "4", "bad"})
            require(OutputIndexPolicy::restoredChoice(value, 3).isEmpty(), "invalid output survived shrink");
        require(OutputIndexPolicy::restoredChoice("2", 3) == "2", "valid output lost");
        using namespace LoadCapabilityCatalog;
        require(supportedManualModes("63101", " cc ") == QStringList{"CCL", "CCH"}, "6310 ranges changed");
        require(supportedManualModes("63101A", "CV") == QStringList{"CV"}, "6310A ranges changed");
        require(supportedManualModes("63610-80-20", "CV") == QStringList{"CVL", "CVM", "CVH"}, "63600 ranges changed");
        require(supportedDynamicManualModes("63202A-150-200") == QStringList{"CCDL", "CCDM", "CCDH"}, "63200 dynamic ranges changed");
        require(supportedManualModes("63804", "CC") == QStringList{"CURR"}, "63800 ranges changed");
        require(supportedDynamicManualModes("unknown").isEmpty(), "unknown device accepted");
        require(supportedManualModes("63101", "invalid").isEmpty(), "unknown mode accepted");
        std::cout << "PASS: output selection and load capability policies\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
