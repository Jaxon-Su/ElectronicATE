#include "instrumentsetupviewmodel.h"
#include "testconditionsviewmodel.h"
#include "manualcontrolviewmodel.h"
#include "xmlconfigstore.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <iostream>
#include <stdexcept>
void check(bool ok) { if (!ok) throw std::runtime_error("InstrumentSetup／TestConditions／ManualControl XML integration failed"); }
int main(int argc, char** argv) {
 QCoreApplication app(argc, argv);
 try {
  InstrumentSetupModel instrumentSetupModel; TestConditionsModel testConditionsModel; ManualControlModel manualControlModel;
  InstrumentSetupViewModel instrumentSetupViewModel(&instrumentSetupModel); TestConditionsViewModel testConditionsViewModel(&testConditionsModel);
  ManualControlOperations ops;
  ops.dcGroup = [](const InstrumentSetupConfig&, const DcGroup&, InputAction) { return InstrumentOperationResult{}; };
  ManualControlViewModel manualControlViewModel(&manualControlModel, ops);
  QObject::connect(&instrumentSetupViewModel, &InstrumentSetupViewModel::configUpdated, &testConditionsViewModel, &TestConditionsViewModel::onInstrumentSetupConfigChanged);
  QObject::connect(&instrumentSetupViewModel, &InstrumentSetupViewModel::configUpdated, &manualControlViewModel, &ManualControlViewModel::onInstrumentSetupConfigChanged);
  QObject::connect(&testConditionsViewModel, &TestConditionsViewModel::conditionsChanged, &manualControlViewModel, &ManualControlViewModel::onConditionsChanged);
  auto sync = [&] {
   const auto c = instrumentSetupViewModel.currentConfig();
   testConditionsViewModel.onInstrumentSetupConfigChanged(c); testConditionsViewModel.setMaxOutput(c.loadOutputs); testConditionsViewModel.setMaxRelayOutput(c.relayOutputs);
   manualControlViewModel.onInstrumentSetupConfigChanged(c); manualControlViewModel.onConditionsChanged(testConditionsViewModel.conditions());
  };
  QTemporaryDir dir; check(dir.isValid());
  for (int count = 1; count <= 3; ++count) {
   InstrumentSetupConfig c; c.dcInputs = count;
   for (int i=1;i<=3;++i) { InstrumentConfig inst; inst.name=QString("DC Source%1").arg(i); inst.type="InputDCSource"; inst.enabled=i!=2; inst.modelName="62050H-40"; inst.commConfig = CommunicationConfig::fromResourceString(QString("GPIB0::%1::INSTR").arg(i)); c.instruments.append(inst); }
   instrumentSetupModel.setConfig(c);
   TestConditionSnapshot s; s.dcNames={"First", QString::fromUtf8("額定 & <DC>")};
   s.dcRows={{"24","5",""},{"30","6",""}};
   s.dcRows2={{"12","3",""},{"15","4",""}}; s.dcRows3={{"5","1",""},{"6","2",""}};
   testConditionsViewModel.setConditions(s); sync(); manualControlViewModel.onDcSelected(0,1,s.dcNames[1]);
   const auto path=dir.filePath("config.xml");
   check(XmlConfigStore::saveAllToXml(path,{&instrumentSetupViewModel,&testConditionsViewModel,&manualControlViewModel}).succeeded());
   instrumentSetupModel.setConfig({}); testConditionsViewModel.setConditions({}); manualControlViewModel.onDcSelected(0,-1,{});
   check(XmlConfigStore::loadAllFromXml(path,{&instrumentSetupViewModel,&testConditionsViewModel,&manualControlViewModel},sync).succeeded());
   check(instrumentSetupViewModel.currentConfig().dcInputs==count && testConditionsViewModel.dcInputs()==count);
   check(!instrumentSetupViewModel.currentConfig().instruments[1].enabled && instrumentSetupViewModel.currentConfig().instruments[2].getResourceString()=="GPIB0::3::INSTR");
   check(testConditionsViewModel.conditions().dcRows3[1].currentLimit=="2" && manualControlModel.dcSelection(0).index==1 && manualControlModel.dcSelection(0).text==s.dcNames[1]);
   check(!manualControlViewModel.hasActiveControl());
   QFile file(path); check(file.open(QIODevice::ReadOnly)); auto xml=file.readAll(); file.close();
   xml.replace("<ManualControl schemaVersion=\"2\">", "<ManualControl schemaVersion=\"1\">");
   check(file.open(QIODevice::WriteOnly)); file.write(xml); file.close();
   check(!XmlConfigStore::loadAllFromXml(path,{&instrumentSetupViewModel,&testConditionsViewModel,&manualControlViewModel},sync).succeeded());
   check(testConditionsViewModel.conditions().dcRows3[1].currentLimit=="2" && manualControlModel.dcSelection(0).index==1);
  }
  std::cout << "PASS: InstrumentSetup／TestConditions／ManualControl XML roundtrip, count, hidden data, selection and atomic rejection\n";
 } catch(const std::exception& e) { std::cerr << e.what(); return 1; }
}
