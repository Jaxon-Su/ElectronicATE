#include "testsequencemodel.h"
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "testsequencesettingsvalidation.h"
#include "testsequencetaskdefaults.h"
#include <iostream>
#include <stdexcept>
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
int main(int argc,char** argv){
 QCoreApplication app(argc,argv);
 if (argc > 1 && QString(argv[1]) == "drafts") {
  for (const auto &descriptor : TestSequenceTaskKind::descriptors) {
   for (bool useDefaults : {false, true}) {
    TestSequenceModel source;
    DutRowData row; row.item = descriptor.name;
    if (useDefaults) row.settings = testSequenceDefaultSettings(row.item);
    source.appendTask(row);
    // Check repeated save/load, including an untouched Write Oscilloscope row.
    for (int pass = 0; pass < 2; ++pass) {
     QString xml; QXmlStreamWriter writer(&xml); source.writeXml(writer);
     require(!writer.hasError(), "editable draft could not be saved");
     QXmlStreamReader reader(xml); reader.readNextStartElement(); source.loadXml(reader);
     if (reader.hasError()) {
      std::cerr << descriptor.name << ": " << reader.errorString().toStdString() << '\n';
      return 1;
     }
     require(source.tasks().size() == 1, "draft roundtrip lost a task");
     // JSON arrays restore as QVariantList even when defaults use QStringList.
     require(QJsonDocument::fromVariant(source.tasks()[0].settings) == QJsonDocument::fromVariant(row.settings),
             "draft roundtrip inserted defaults or changed settings");
    }
   }
  }
  return 0;
 }
 TestSequenceModel model;
 const QStringList names{"Static Test","Dynamic Test","Turn on","Turn off","Turn on then short","Short then turn on","Relay","Write Oscilloscope","Delay"};
 const QStringList keys{"static","dynamic","turnOn","turnOff","onShort","shortOn","relay","osc","delay"};
 for(int i=0;i<names.size();++i){
  DutRowData row;row.item=names[i];row.ext="測試 <A&B>";row.active=i%2;row.report=!(i%2);row.retry=QString::number(i);
  QVariantMap cfg;
  if (keys[i] == "osc") cfg = {{"timescale","20ms"}};
  else if (keys[i] == "delay") cfg = {{"delay_ms",5000}};
  else if (keys[i] == "relay") cfg = {{"relay_index",-1}};
  else cfg = {{"ac_input_index",-1},{"condition_refs",QVariantMap{{"ac_input","DC:48V&5A"}}}};
  row.settings={{keys[i],cfg}};model.appendTask(row);
 }
 model.reportName="測試 & report"; model.reportDirectory="C:/reports & data";
 QString xml;QXmlStreamWriter writer(&xml);model.writeXml(writer);
 TestSequenceModel loaded;QXmlStreamReader reader(xml);reader.readNextStartElement();loaded.loadXml(reader);
 require(!reader.hasError()&&loaded.tasks().size()==9,"roundtrip failed");
 require(loaded.reportName==model.reportName && loaded.reportDirectory==model.reportDirectory,"report file settings lost");
 {
   QString removed=xml;removed.replace("<Item>Static Test</Item>","<Item>Capture</Item>");
   require(removed!=xml,"removed-task fixture did not change XML");
   QXmlStreamReader old(removed);old.readNextStartElement();loaded.loadXml(old);
   require(old.hasError() && loaded.tasks().size()==9 && loaded.tasks()[0].item=="Static Test",
           "removed Capture task accepted or failed load changed current tasks");
 }
 for(const QString tag:{"TestSequence","DutTable","Row","Item","Ext","Retry","Settings"}) {
   QString bad=xml;const QString opening="<"+tag;const auto pos=bad.indexOf(opening);
   require(pos>=0,"attribute test tag missing");bad.insert(pos+opening.size()," Unexpected='value'");
   const auto before=loaded.tasks();int emitted=0;
   const auto connection=QObject::connect(&loaded,&TestSequenceModel::configLoaded,[&]{++emitted;});
   QXmlStreamReader in(bad);in.readNextStartElement();loaded.loadXml(in);
   require(in.hasError()&&emitted==0&&loaded.tasks().size()==before.size()&&
           loaded.tasks()[0].uid==before[0].uid&&loaded.tasks()[0].settings==before[0].settings&&
           loaded.reportName==model.reportName&&loaded.reportDirectory==model.reportDirectory,
           "unexpected attribute accepted or invalid load changed existing configuration");
   QObject::disconnect(connection);
 }
 for (const QString& invalid : {QString("<TestSequence><ReportFile Name='bad'/><ReportFile/><DutTable/></TestSequence>"),
      QString("<TestSequence><ReportFile><Bad/></ReportFile><DutTable/></TestSequence>")}) {
   QXmlStreamReader bad(invalid); bad.readNextStartElement(); loaded.loadXml(bad);
   require(bad.hasError() && loaded.reportName==model.reportName && loaded.reportDirectory==model.reportDirectory,"report invalid load was not atomic");
 }

 for(int i=0;i<9;++i){const auto& a=model.tasks()[i];const auto& b=loaded.tasks()[i];
 require(a.item==b.item&&a.ext==b.ext&&a.active==b.active&&a.report==b.report&&a.retry==b.retry,"row lost");
 require(QJsonDocument::fromVariant(a.settings)==QJsonDocument::fromVariant(b.settings),"nested settings lost");}
 std::cout<<"PASS all nine nested settings and table fields roundtrip\n";

 for (const QString& name : {"PNG", "CSV", "AllCSV", "WFM", "AllWFM"}) {
  TestSequenceModel captureModel; DutRowData row; row.item="Static Test"; row.retry="0";
  row.settings={{"static", QVariantMap{{"capture_enabled",true},{"capture_settings",QVariantMap{{"directory","C:/capture & test"},{"channel",8},{"formats",QStringList{name}}}}}}};
  captureModel.appendTask(row);
  QString text; QXmlStreamWriter out(&text); captureModel.writeXml(out);
  TestSequenceModel restored; QXmlStreamReader in(text); in.readNextStartElement(); restored.loadXml(in);
  require(!in.hasError() && restored.tasks().size()==1, "capture XML load");
  require(QJsonDocument::fromVariant(restored.tasks()[0].settings)==QJsonDocument::fromVariant(row.settings), "capture XML settings lost");
  require(restored.tasks()[0].settings.value("static").toMap().value("capture_settings").toMap().value("formats").toStringList()==QStringList{name}, "capture format list conversion failed");
 }
 int notifications=0;
 QObject::connect(&loaded,&TestSequenceModel::configLoaded,[&]{++notifications;});
 for(const QString& json:{"{bad}","[]","{&quot;osc&quot;:true}",
 "{&quot;osc&quot;:{&quot;auto_period&quot;:&quot;true&quot;}}",
 "{&quot;static&quot;:{&quot;condition_refs&quot;:[]}}",
 "{&quot;static&quot;:{&quot;condition_refs&quot;:{&quot;input&quot;:5}}}"}){
 QString bad="<TestSequence><DutTable><Row Active=\"1\" Report=\"1\"><Item>Write Oscilloscope</Item><Retry>0</Retry><Settings>"+json+"</Settings></Row></DutTable></TestSequence>";
 QXmlStreamReader input(bad);input.readNextStartElement();loaded.loadXml(input);
 require(input.hasError(),"malformed Settings accepted");
 require(loaded.tasks().size()==9&&notifications==0,"failed load mutated data or emitted configLoaded");
 }
 auto checkLoad = [&](const QString& item, const QString& json, bool valid,
                      const QString& retry = "0", bool includeItem = true) {
  QString text; QXmlStreamWriter out(&text);
  out.writeStartElement("TestSequence");
  out.writeStartElement("ReportFile"); out.writeAttribute("Name", ""); out.writeAttribute("Directory", ""); out.writeEndElement();
  out.writeStartElement("DutTable"); out.writeStartElement("Row");
  out.writeAttribute("Active","1"); out.writeAttribute("Report","1");
  if(includeItem) out.writeTextElement("Item",item);
  out.writeTextElement("Ext", "");
  if(!retry.isNull()) out.writeTextElement("Retry",retry);
  out.writeTextElement("Settings",json);
  out.writeEndElement(); out.writeEndElement(); out.writeEndElement();
  const auto before = loaded.tasks(); const int emitted = notifications;
  QXmlStreamReader in(text); in.readNextStartElement(); loaded.loadXml(in);
  require(in.hasError()!=valid,"schema accepted invalid input or rejected valid draft");
  if(!valid) require(loaded.tasks().size()==before.size() && notifications==emitted
      && loaded.tasks()[0].item==before[0].item && loaded.tasks()[0].settings==before[0].settings,
      "schema failure mutated existing state");
 };
 checkLoad("Static Test",R"({"osc":{}})",false);
 checkLoad("Unknown","{}",false);
 checkLoad("Delay","{}",false,"0",false);
 checkLoad("Delay","{}",false,QString());
 for(const QString& retry : {"-1","101","1.5","oops"}) checkLoad("Delay","{}",false,retry);
 checkLoad("Delay",R"({"delay":{"delay_ms":1,"delay_ms":2}})",false);
 checkLoad("Delay",R"({"delay":{"delay_ms":1,"\u0064elay_ms":2}})",false);
 checkLoad("Delay",R"({"delay":{},"delay":{}})",false);
 for(const QString &group : {"static", "dynamic", "turnOn", "turnOff", "onShort", "shortOn", "brownOut"}) {
  const auto item = group=="brownOut" ? QString("Brown Out") : names[keys.indexOf(group)];
  for(const QString &selection : {R"("ac_input_index":0,"dc_input_index":-1)",
                                  R"("ac_input_index":-1,"dc_input_index":0)",
                                  R"("ac_input_index":0,"dc_input_index":1)"}) {
   const auto json = QString("{\"%1\":{%2}}").arg(group,selection);
   checkLoad(item,json,true);
   require(QJsonObject::fromVariantMap(loaded.tasks()[0].settings)==QJsonDocument::fromJson(json.toUtf8()).object(),"AC/DC indices changed on XML load");
  }
  for(const QString &invalid : {R"("input_index":0)",R"("input_label":"old")",R"("condition_refs":{"input":"old"})",
                                R"("dc_input_index":true)",R"("dc_input_index":0.5)",R"("dc_input_index":"0")",R"("dc_input_index":-2)"})
   checkLoad(item,QString("{\"%1\":{%2}}").arg(group,invalid),false);
 }
 for(const QString &ramp : {"AC","DC"}) checkLoad("Brown Out",QString("{\"brownOut\":{\"ac_input_index\":0,\"dc_input_index\":0,\"ramp_source\":\"%1\"}}").arg(ramp),true);
 checkLoad("Brown Out",R"({"brownOut":{"ramp_source":"BOTH"}})",false);
 for (bool ignoreAdvanced : {false, true}) {
  for (double threshold : {0.0, 0.125, 0.5, 1.75, 1000.0}) {
   checkLoad("Brown Out", QString("{\"brownOut\":{\"shutdown_voltage\":%1,\"ignore_advanced\":%2}}")
       .arg(threshold, 0, 'g', 16).arg(ignoreAdvanced ? "true" : "false"), true);
   const BrownOut::Settings settings(loaded.tasks()[0].settings.value("brownOut").toMap());
   require(settings.shutdownVoltage == threshold, "XML shutdown voltage replaced by a fixed default");
  }
 }
 for (const QString value : {"-0.1", "1000.1", "true", "\"0.5\""})
  checkLoad("Brown Out", QString("{\"brownOut\":{\"shutdown_voltage\":%1}}").arg(value), false);
 for(const QString& json : {
     R"({"static":{"ac_input_index":"1"}})", R"({"static":{"ac_input_index":1.5}})",
     R"({"static":{"ac_input_index":-2}})", R"({"static":{"delay_ms":3600001}})",
     R"({"static":{"condition_refs":{"unknown":"x"}}})",
     R"({"static":{"typo":1}})"}) checkLoad("Static Test",json,false);
 for(const QString& json : {
     R"({"turnOn":{"trig_timeout_ms":0}})", R"({"turnOn":{"trials_per_level":11}})",
     R"({"turnOn":{"phase_timeout_ms":999}})", R"({"turnOn":{"level_step_v":0}})",
     R"({"turnOn":{"search_tolerance_v":1001}})", R"({"turnOn":{"discharge_ms":0}})",
     R"({"turnOn":{"measure_target":"INVALID"}})", R"({"turnOn":{"trigger_edge":"RISE"}})"})
     checkLoad("Turn on",json,false);
 for(const QString& json : {
     R"({"osc":{"timescale":{}}})", R"({"osc":{"timescale":20}})",
     R"({"osc":{"timescale":"0ms"}})", R"({"osc":{"timescale":"20A"}})",
     R"({"osc":{"horz_pos":101}})", R"({"osc":{"fastacq":1}})",
     R"({"osc":{"record_length":"0.5"}})", R"({"osc":{"sample_rate":"bad"}})",
     R"({"osc":{"ch1_enabled":"true"}})", R"({"osc":{"ch9_enabled":true}})",
     R"({"osc":{"ch1_position":9}})", R"({"osc":{"trig_source":"CH99"}})"})
     checkLoad("Write Oscilloscope",json,false);

 for(const QString key : {"level_step_v", "search_tolerance_v"})
    checkLoad("Turn on", QString("{\"turnOn\":{\"%1\":0.5}}").arg(key), false);
 for (const QString name : {"Static Test","Dynamic Test","Static Group Test","Dynamic Group Test"}) {
    const auto group = TestSequenceTaskKind::settingsGroup(name);
    for (const QString key : {"power_off_ms","startup_ms"}) {
        for (const QString value : {"0","1234","3600000"})
            checkLoad(name,QString("{\"%1\":{\"%2\":%3}}").arg(group,key,value),true);
        for (const QString value : {"-1","3600001","1.5","true","\"100\""})
            checkLoad(name,QString("{\"%1\":{\"%2\":%3}}").arg(group,key,value),false);
    }
 }
 checkLoad("Turn on",R"({"turnOn":{"power_off_ms":5000}})",false);
 for (const auto &descriptor : TestSequenceTaskKind::descriptors) {
  const auto kind = descriptor.kind;
  if (!TestSequenceTaskKind::isSteady(kind) &&
      !(TestSequenceTaskKind::isTransient(kind) && !TestSequenceTaskKind::isDescent(kind))) continue;
  const QString group = descriptor.group;
  for (const QString value : {"1","2","8"}) {
   checkLoad(descriptor.name,QString("{\"%1\":{\"trigger_channel\":%2}}").arg(group,value),true);
   require(loaded.tasks()[0].settings.value(group).toMap().value("trigger_channel").toInt()==value.toInt(),
           "XML changed explicit trigger channel");
  }
  for (const QString value : {"0","9","1.5","true","\"2\"","null"})
   checkLoad(descriptor.name,QString("{\"%1\":{\"trigger_channel\":%2}}").arg(group,value),false);
  checkLoad(descriptor.name,QString("{\"%1\":{\"trigger_channel\":1,\"trigger_channel\":2}}").arg(group),false);
  checkLoad(descriptor.name,QString("{\"%1\":{}}").arg(group),true);
  require(!loaded.tasks()[0].settings.value(group).toMap().contains("trigger_channel"),"legacy XML invented a source");
 }
 // New per-task options: exhaustive boolean/direction round trips, no cross-task carryover.
 for(int task=0;task<6;++task) {
  for(int flags=0;flags<(task<2?8:4);++flags) {
   for(const QString direction : {"BOTH","MAX","MIN"}) {
    QVariantMap cfg{{"measure_target",direction}};
    cfg["vertical_margin"]=bool(flags&2);
    if(task<2)cfg["auto_period"]=bool(flags&4);
    cfg["capture_enabled"]=bool(flags&1);
    cfg["capture_settings"]=QVariantMap{{"formats",QVariantList{"WFM","PNG"}},{"directory","C:/test & capture"},{"channel",0}};
    TestSequenceModel source; DutRowData row;row.item=names[task];row.retry="0";row.settings={{keys[task],cfg}};
    source.appendTask(row);
    QString text;QXmlStreamWriter out(&text);source.writeXml(out);require(!out.hasError(),"new options save rejected");
    TestSequenceModel restored;QXmlStreamReader in(text);in.readNextStartElement();restored.loadXml(in);
    require(!in.hasError()&&restored.tasks().size()==1,"new options load rejected");
    require(restored.tasks()[0].settings.value(keys[task]).toMap()==cfg,"new options changed during roundtrip");
   }
  }
  for(const QString key : {"auto_vertical_scale","auto_period","capture_enabled","vertical_margin"}) {
   const bool allowed=key!="auto_vertical_scale" && (key!="auto_period"||task<2);
   for(const QJsonValue value : {QJsonValue(true),QJsonValue(false),QJsonValue("true"),QJsonValue(1),QJsonValue(QJsonValue::Null)}) {
    QJsonObject group{{key,value}};QJsonObject settings{{keys[task],group}};
    checkLoad(names[task],QString::fromUtf8(QJsonDocument(settings).toJson(QJsonDocument::Compact)),allowed&&value.isBool());
   }
  }
 }
 for (const auto &descriptor : TestSequenceTaskKind::descriptors) {
  const QString name=descriptor.name;
  const auto kind=TestSequenceTaskKind::fromName(name);
  const bool allowed=TestSequenceTaskKind::isSteady(kind) ||
      (TestSequenceTaskKind::isTransient(kind) && !TestSequenceTaskKind::isDescent(kind));
  const auto group=TestSequenceTaskKind::settingsGroup(name);
  for (const QString value : {"true","false","1","\"false\"","null"})
   checkLoad(name,QString("{\"%1\":{\"vertical_margin\":%2}}").arg(group,value),
             allowed && (value=="true" || value=="false"));
 }
 checkLoad("Write Oscilloscope",R"({"osc":{"auto_period":true}})",false);
 checkLoad("Write Oscilloscope",R"({"osc":{"auto_position":true}})",false);
 checkLoad("Relay",R"({"relay":{"relay_values":[1]}})",false);
 checkLoad("Delay","{}",true);
 checkLoad("Static Test",R"({"static":{"ac_input_index":-1,"load_index":-1}})",true);
 checkLoad("Write Oscilloscope",R"({"osc":{"ch1_scale":"2mA","timescale":"20ms","record_length":"62.5M"}})",true);
 QString error;
 require(TestSequenceSettingsValidation::uniqueKeys(R"({"delay":{"text":"escaped \" key: {}"}})"),
         "string content confused duplicate-key scanner");
 TestSequenceModel invalidSave; DutRowData bad; bad.item="Delay"; bad.settings={{"delay",QVariantMap{{"delay_ms",-1}}}};
 invalidSave.appendTask(bad);
 QString rejected; QXmlStreamWriter badWriter(&rejected); invalidSave.writeXml(badWriter);
 require(badWriter.hasError(),"invalid in-memory config serialized successfully");
}
