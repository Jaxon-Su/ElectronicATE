#include "outputindexpolicy.h"
#include "testsequenceconditionmapping.h"
#include "channelnumberpolicy.h"
#include "loadcapabilitycatalog.h"
#include "instrumentsetupconfig.h"
#include <iostream>
#include <stdexcept>

void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

int main()
{
    try {
        TestSequenceExecutionContext ctx;
        ctx.inputs.append(InputRow{});
        DcGroup group; group[0].vin="48"; group[0].currentLimit="5";
        ctx.dcInputs.append(group); ctx.dcLabels.append("DC48");
        QVariantMap settings{{"dc_input_index",0}};
        TestSequenceConditions::capture(settings,ctx);
        require(TestSequenceConditions::inputOptions(ctx).dc.last()=="DC48","DC option missing");
        ctx.inputs.prepend(InputRow{});
        QString error;
        require(TestSequenceConditions::resolve(settings,ctx,error)&&settings["dc_input_index"].toInt()==0,"DC reference moved to AC after insert");
        ctx.dcInputs[0][0].vin="50";
        require(!TestSequenceConditions::resolve(settings,ctx,error),"changed DC condition accepted");
        TaskPayload payload; payload.task.name="Delay";
        for(int retry:{-1,101}){payload.retry=retry;require(!TestSequenceConditions::validate(payload,ctx,error),"invalid retry accepted");}
        payload.retry=100;require(TestSequenceConditions::validate(payload,ctx,error),"valid retry rejected");
        TestSequenceExecutionContext blank;
        blank.inputs.append(InputRow{});blank.dcInputs.append(DcGroup{});blank.dcLabels.append("named only");
        blank.loads.append(LoadDataRow{});blank.dynamics.append(DynamicDataRow{});
        require(TestSequenceConditions::inputOptions(blank).ac==QStringList({""})&&TestSequenceConditions::inputOptions(blank).dc==QStringList({""}),"blank input exposed");
        require(TestSequenceConditions::rowOption("name",{}).isEmpty(),"name-only load exposed");
        require(!TestSequenceConditions::rowOption("",{"0"}).isEmpty(),"zero setting incorrectly excluded");
        QVariantMap emptyCfg{{"dc_input_index",0}};TestSequenceConditions::capture(emptyCfg,blank);
        TestSequenceConditions::prepareDialog(emptyCfg,blank);
        require(emptyCfg["dc_input_index"].toInt()==-1,"blank saved input restored");
        TestSequenceExecutionContext dual;
        dual.inputs={{"1phase","90","50","0"},{"1phase","100","60","0"}};
        dual.dcInputs={group,group}; dual.dcInputs[1][0].vin="24";
        dual.dcLabels={"DC48","DC24"}; dual.loads={{"L1",{"1"}}};
        QVariantMap selected{{"ac_input_index",0},{"dc_input_index",1},{"load_index",0}};
        TestSequenceConditions::capture(selected,dual);
        std::swap(dual.inputs[0],dual.inputs[1]);
        require(TestSequenceConditions::resolve(selected,dual,error)&&selected["ac_input_index"]==1&&selected["dc_input_index"]==1,"AC reorder shifted DC");
        std::swap(dual.dcInputs[0],dual.dcInputs[1]); std::swap(dual.dcLabels[0],dual.dcLabels[1]);
        require(TestSequenceConditions::resolve(selected,dual,error)&&selected["ac_input_index"]==1&&selected["dc_input_index"]==0,"DC reorder shifted AC");
        dual.inputs.removeAt(1);
        auto draft=selected; TestSequenceConditions::prepareDialog(draft,dual);
        require(draft["ac_input_index"]==-1&&draft["dc_input_index"]==0,"missing AC cleared DC");
        TaskPayload stale; stale.task.name="Static Test"; stale.settings=selected;
        require(!TestSequenceConditions::validate(stale,dual,error),"removed AC silently switched to DC-only at Run");
        TestSequenceConditions::capture(draft,dual); stale.settings=draft;
        require(TestSequenceConditions::validate(stale,dual,error),"explicit DC-only reselection rejected");
        InstrumentConfig configured;
        require(configured.getResourceString().isEmpty(), "unconfigured resource must be empty");
        configured.commConfig.protocol = ProtocolType::GPIB;
        configured.commConfig.gpibAddress = 8;
        require(configured.getResourceString() == "GPIB0::8::INSTR", "structured resource did not override stale address");
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
