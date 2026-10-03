#include "page1viewmodel.h"
#include "page2viewmodel.h"
#include "page3viewmodel.h"
#include "xmlconfigstore.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <iostream>
#include <stdexcept>
void check(bool ok) { if (!ok) throw std::runtime_error("Page1-3 XML integration failed"); }
int main(int argc, char** argv) {
 QCoreApplication app(argc, argv);
 try {
  Page1Model m1; Page2Model m2; Page3Model m3;
  Page1ViewModel v1(&m1); Page2ViewModel v2(&m2);
  Page3Operations ops;
  ops.dcGroup = [](const Page1Config&, const DcGroup&, InputAction) { return InstrumentOperationResult{}; };
  Page3ViewModel v3(&m3, ops);
  QObject::connect(&v1, &Page1ViewModel::configUpdated, &v2, &Page2ViewModel::onPage1ConfigChanged);
  QObject::connect(&v1, &Page1ViewModel::configUpdated, &v3, &Page3ViewModel::onPage1ConfigChanged);
  QObject::connect(&v2, &Page2ViewModel::conditionsChanged, &v3, &Page3ViewModel::onConditionsChanged);
  auto sync = [&] {
   const auto c = v1.currentConfig();
   v2.onPage1ConfigChanged(c); v2.setMaxOutput(c.loadOutputs); v2.setMaxRelayOutput(c.relayOutputs);
   v3.onPage1ConfigChanged(c); v3.onConditionsChanged(v2.conditions());
  };
  QTemporaryDir dir; check(dir.isValid());
  for (int count = 1; count <= 3; ++count) {
   Page1Config c; c.dcInputs = count;
   for (int i=1;i<=3;++i) { InstrumentConfig inst; inst.name=QString("DC Source%1").arg(i); inst.type="InputDCSource"; inst.enabled=i!=2; inst.modelName="62050H-40"; inst.address=QString("GPIB0::%1::INSTR").arg(i); c.instruments.append(inst); }
   m1.setConfig(c);
   TestConditionSnapshot s; s.dcNames={"First", QString::fromUtf8("額定 & <DC>")};
   s.dcRows={{"24","5",""},{"30","6",""}};
   s.dcRows2={{"12","3",""},{"15","4",""}}; s.dcRows3={{"5","1",""},{"6","2",""}};
   v2.setConditions(s); sync(); v3.onDcSelected(0,1,s.dcNames[1]);
   const auto path=dir.filePath("config.xml");
   check(XmlConfigStore::saveAllToXml(path,{&v1,&v2,&v3}).succeeded());
   m1.setConfig({}); v2.setConditions({}); v3.onDcSelected(0,-1,{});
   check(XmlConfigStore::loadAllFromXml(path,{&v1,&v2,&v3},sync).succeeded());
   check(v1.currentConfig().dcInputs==count && v2.dcInputs()==count);
   check(!v1.currentConfig().instruments[1].enabled && v1.currentConfig().instruments[2].address=="GPIB0::3::INSTR");
   check(v2.conditions().dcRows3[1].currentLimit=="2" && m3.dcSelection(0).index==1 && m3.dcSelection(0).text==s.dcNames[1]);
   check(!v3.hasActiveControl());
   QFile file(path); check(file.open(QIODevice::ReadOnly)); auto xml=file.readAll(); file.close();
   xml.replace("<Page3 schemaVersion=\"2\">", "<Page3 schemaVersion=\"1\">");
   check(file.open(QIODevice::WriteOnly)); file.write(xml); file.close();
   check(!XmlConfigStore::loadAllFromXml(path,{&v1,&v2,&v3},sync).succeeded());
   check(v2.conditions().dcRows3[1].currentLimit=="2" && m3.dcSelection(0).index==1);
  }
  std::cout << "PASS: Page1-3 XML roundtrip, count, hidden data, selection and atomic rejection\n";
 } catch(const std::exception& e) { std::cerr << e.what(); return 1; }
}
