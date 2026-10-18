#include "../../support/transient_test_support.h"
#include "oscilloscopeacquisition.h"
class StateScope : public Scope {
public:
 QStringList states; int index=0, reads=0, completeAfter=1000;
 QAtomicInt* cancel=nullptr;
 QString getTriggerState() override {
  if(cancel)cancel->storeRelease(1);
  return states[qMin(index++,states.size()-1)];
 }
 QString getAcquisitionState() override {return ++reads>=completeAfter?"0":"1";}
};
class BelowZeroScope : public Scope {
public:
 QString getAcquisitionState() override {
  if (!autoMode && negative && level==0) return "1";
  return Scope::getAcquisitionState();
 }
};
int main(int argc,char**argv){
 QCoreApplication app(argc,argv);QAtomicInt stop(0);
 for(auto kind:{TransientKind::TurnOn,TransientKind::ShortThenTurnOn}){
  Scope small;small.single=true;small.peak=.2;small.missMin=true;
  auto c=context(small);auto r=runTransientStrategy(kind,c,&small,stop);
  require(r.success && r.channels[0].maxVal==.2,"small positive excursion still missed");
  std::cout<<"small positive kind="<<int(kind)<<" success="<<r.success<<" first="<<small.levels.first()<<" levels=";
  for(double v:small.levels)std::cout<<v<<",";
  std::cout<<" error="<<r.errorMessage.toStdString()<<"\n";
  BelowZeroScope negative;negative.single=true;negative.missMax=true;
  auto nc=context(negative);auto nr=runTransientStrategy(kind,nc,&negative,stop);
  bool below=false;for(double v:negative.levels)below|=v<0;
  require(nr.success && below,"negative event still missed");
  std::cout<<"no zero-crossing kind="<<int(kind)<<" success="<<nr.success<<" tested_negative_level="<<below<<"\n";
 }
 using namespace OscilloscopeAcquisition;
 QString error;
 StateScope ready;ready.states={"ARMED",":TRIGGER:STATE READY"};
 require(waitForReady(&ready,stop,100,error),"readiness transition failed");
 for(const auto& state:{"TRIGGER","SAVE","AUTO","invalid"}) {
  StateScope bad;bad.states={state};
  require(!waitForReady(&bad,stop,20,error),"unsafe state allowed event");
 }
 StateScope arming;arming.states={"ARMED"};
 require(!waitForReady(&arming,stop,2,error),"arming timeout ignored");
 StateScope cancelled;cancelled.states={"READY"};cancelled.cancel=&stop;
 require(!waitForReady(&cancelled,stop,100,error),"cancel during query ignored");stop.storeRelease(0);
 StateScope slow;slow.states={"TRIGGER"};slow.completeAfter=3;
 require(waitForCapture(&slow,stop,5,100)==AcquisitionResult::Completed,"post-trigger completion not awaited");
 StateScope stuck;stuck.states={"TRIGGER"};
 require(waitForCapture(&stuck,stop,2,5)==AcquisitionResult::CaptureTimeout,"triggered timeout misclassified as MISS");
 StateScope notReady;notReady.states={"ARMED"};
 require(waitForCapture(&notReady,stop,2)==AcquisitionResult::CaptureTimeout,"arming timeout misclassified as MISS");
 StateScope miss;miss.states={"READY"};
 require(waitForCapture(&miss,stop,2)==AcquisitionResult::NoTrigger,"ready timeout not classified as MISS");
 std::cout<<"PASS readiness and trigger completion cases\n";
}
