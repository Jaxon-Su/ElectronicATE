#include "../../support/transient_test_support.h"
int main(int argc,char**argv){QCoreApplication app(argc,argv);try{
    QAtomicInt stop(0);
    Scope rejected;rejected.failWrite="TRIGger:A:TYPe EDGE";auto rc=context(rejected);
    require(!runTransientStrategy(TransientKind::TurnOn,rc,&rejected,stop).success&&rejected.arms==0,"SCPI rejection ignored");
    for(const auto& name:{"Turn on","Turn off","Turn on then short","Short then turn on"}){
        std::unique_ptr<IOscilloscopeMeasureStrategy>s(OscilloscopeStrategyFactory::create(name,&rc));
        require(bool(s)&&OscilloscopeStrategyFactory::create(name)==nullptr,"factory gap");
    }
    std::cout<<"binary transient strategies passed\n";
}catch(const std::exception&e){std::cerr<<e.what();return 1;}}
