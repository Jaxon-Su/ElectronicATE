#include "testrunservice.h"
#include "eventsearchbracket.h"
#include "icommunication.h"
#include <QCoreApplication>
#include <iostream>
#include <stdexcept>
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
class Transport : public ICommunication {
public:
    int end = 0, calls = 0;
    bool open() override { return true; }
    void close() override {}
    int write(const QByteArray& b) override { return b.size(); }
    int read(QByteArray& data, int) override {
        if (++calls == 1) { data = "abc"; return 3; }
        data.clear(); return end;
    }
    bool isOpen() const override { return true; }
    QString lastError() const override { return {}; }
};
int main(int argc, char** argv) {
 QCoreApplication app(argc,argv);
 try {
    const QString scenario=argc>1?argv[1]:"";
    if(scenario=="capture_options") {
        const CaptureOptions valid({{"formats",QStringList{"PNG","WFM"}},{"channel",2}});
        require(valid.channel==2 && valid.formats.size()==2,"capture settings changed");
        for(const QVariant &channel : {QVariant(true),QVariant("1"),QVariant(1.2),QVariant(9)}) {
            bool rejected=false;try { CaptureOptions invalid({{"formats",QStringList{"PNG"}},{"channel",channel}}); }catch(const std::invalid_argument &){rejected=true;}
            require(rejected,"invalid Capture channel accepted");
        }
        bool rejected=false;try { CaptureOptions invalid({{"formats",QStringList{"PNG","PNG"}}}); }catch(const std::invalid_argument &){rejected=true;}
        require(rejected,"duplicate Capture accepted");
    } else if(scenario=="typed_preflight") {
        using namespace TaskSettings;
        require(std::holds_alternative<Steady>(decode("Static Test",{}).settings),"wrong steady type");
        require(std::holds_alternative<Transient>(decode("Turn on",{}).settings),"wrong transient type");
        for(const auto &bad:QVariantList{"true",1,0.5}) {
            bool rejected=false;try { decode("Static Test",{{"auto_period",bad}}); } catch(...) {rejected=true;}
            require(rejected,"boolean coercion accepted");
        }
        int writes=0;TestRunDependencies dependencies; dependencies.validateMeasurement = [](const auto &, auto, int, bool) {};
        dependencies.instruments.relay=[&](const InstrumentSetupConfig&,const QVector<RelayDataRow>&,int,RelayAction){++writes;return InstrumentOperationResult{};};
        TestRunService service(dependencies);
        TaskPayload relay;relay.task.name="Relay";relay.settings={{"relay_index",0}};
        TaskPayload delay;delay.task.name="Delay";delay.settings={{"delay_ms",-1}};
        TestSequenceExecutionContext context;context.relays.append(RelayDataRow{});
        QVector<TestSequenceResultRecord> records;
        QObject::connect(&service,&TestRunService::resultRecordsReady,[&](const auto &r){records=r;});
        service.prepareRun();service.startTasks({relay,delay},context);
        require(writes==0 && records.size()==2 && records[0].status=="Skipped" && records[1].attempt==0,"invalid later task reached hardware");
    } else if(scenario=="transport") {
        for(int end:{0,-1,100}) {
            Transport transport;transport.end=end;QByteArray data;
            require(transport.readFully(data)==(end==0),"partial transport outcome wrong");
            require(data==(end==0?QByteArray("abc"):QByteArray{}),"partial bytes escaped");
        }
    } else if(scenario=="settings") {
        for(auto text:{"BOTH","MAX","MIN"}) {
            TaskSettings::Measurement config({{"measure_target",text},{"dyload_index",7}},true);
            require(config.load==7 && TaskSettings::serialized(config.searchDirection)==text,"settings codec");
        }
        for(const QVariant& bad:QVariantList{"bad",true,1.5,1e100}) {
            bool rejected=false;
            try { TaskSettings::integer({{"delay_ms",bad}},"delay_ms",5000); } catch(...) {rejected=true;}
            require(rejected,"malformed integer accepted");
        }
        require(TaskSettings::integer({},"delay_ms",5000)==5000,"default changed");
    } else if(scenario=="bracket") {
        for(double peak:{0.02,0.1,1.0,10.0,55.68,100.0,1000.0})
        for(double step:{0.001,0.04,0.4,4.0})
        for(double factor:{0.2,1.0,2.0,10.0}) {
            EventSearchBracket search(peak*factor,step);
            auto status=EventSearchBracket::Progress::Continue;
            int iterations=0;
            while(status==EventSearchBracket::Progress::Continue && ++iterations<100) {
                require(std::abs(search.candidate()/step-std::round(search.candidate()/step))<1e-8,"fractional candidate");
                status=search.observe(search.candidate()<=peak,peak,0);
            }
            if(peak<step){require(status==EventSearchBracket::Progress::NoEvent,"sub-Step event invented");continue;}
            require(status==EventSearchBracket::Progress::Converged,"search did not converge");
            require(search.hit()<=peak && search.miss()>peak && search.miss()-search.hit()<=step+1e-10,"wrong bracket");
        }
        EventSearchBracket missing(1,0.04);
        const double misses[]={1,.48,.24,.12,.04};
        for(int i=0;i<5;++i) {
            require(std::abs(missing.candidate()-misses[i])<1e-10,"integer bisection changed");
            require(missing.observe(false,0,0)==(i==4?EventSearchBracket::Progress::NoEvent:EventSearchBracket::Progress::Continue),"unbounded MISS");
        }
        EventSearchBracket changed(1,.04);changed.observe(true,1,0);
        bool failed=false;try{changed.baseline(2);}catch(...){failed=true;}
        require(failed,"changed baseline reported success");
        changed.reset(.4);require(!changed.bounded(),"rescale retained bracket");
        require(changed.baseline(2)==EventSearchBracket::Progress::Continue && std::abs(changed.candidate()-2.4)<1e-10,"new baseline");
        for(double bad:{0.,-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
            bool rejected=false;try{TriggerLevelGrid grid(bad);}catch(const std::invalid_argument&){rejected=true;}
            require(rejected,"invalid grid step accepted");
        }
        TriggerLevelGrid fine(.39996);
        require(std::abs(fine.above(21.555)-21.59784)<1e-10,"fine grid truncated");
        for(double bad:{1e29,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()}) {
            bool rejected=false;try{fine.above(bad);}catch(const std::runtime_error&){rejected=true;}
            require(rejected,"unsafe grid value accepted");
        }
    } else {
        int on=0,off=0,finished=0;QVector<TestSequenceResultRecord> records;
        TestRunDependencies dependencies; dependencies.validateMeasurement = [](const auto &, auto, int, bool) {};
        dependencies.instruments.relay=[&](const InstrumentSetupConfig&,const QVector<RelayDataRow>&,int,RelayAction action){
            if(action==RelayAction::RelayOn) ++on; else ++off;
            if(scenario=="cleanup_failure" && action==RelayAction::RelayOff) return InstrumentOperationResult{false,"off failed"};
            return InstrumentOperationResult{};
        };
        TestRunService service(dependencies);
        QObject::connect(&service,&TestRunService::resultRecordsReady,[&](const auto& value){records=value;});
        QObject::connect(&service,&TestRunService::finished,[&]{++finished;});
        TestSequenceExecutionContext context;context.relays.append(RelayDataRow{});
        QVector<TaskPayload> tasks{{{1,"Relay"},{{"relay_index",0}},"",0,true},{{2,"Delay"},{{"delay_ms",0}},"",0,true}};
        if(scenario=="missing_dependency") tasks.append({{3,"Write Oscilloscope"},{},"",0,true});
        if(scenario=="cancel") service.stop();
        service.startTasks(tasks,context);
        require(finished==1 && records.size()==tasks.size(),"result lifecycle");
        if(scenario=="missing_dependency" || scenario=="cancel") require(on==0&&off==0,"preflight/cancel touched hardware");
        else {
            require(on==1&&off==1,"relay lifecycle changed");
            require(records[0].status==(scenario=="cleanup_failure"?"Fail":"Pass"),"cleanup status lost");
            service.prepareRun();service.startTasks({tasks[1]},context);
            require(finished==2&&records.size()==1&&records[0].status=="Pass","run state leaked");
        }
    }
    return 0;
 } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
