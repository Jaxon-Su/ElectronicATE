#include "groupedconditionsmodel.h"
#include <QCoreApplication>
#include <QXmlStreamReader>
#include <stdexcept>
#include <iostream>
void check(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    try {
        TestConditionsModel model; TestConditionsViewModel vm(&model); vm.setMaxOutput(2);
        GroupedConditionsModel load(&vm, TableKind::Load), dynamic(&vm, TableKind::DyLoad), dc(&vm, TableKind::Dc);
        load.append(); dynamic.append(); dc.append();
        check(load.rowCount()==6 && dynamic.rowCount()==4 && dc.rowCount()==2,"group geometry");
        check(load.setData(load.index(0,2),"CV") && vm.loadMeta().modes[0]=="CV","mode commit");
        check(!load.setData(load.index(0,2),"bad") && !load.setData(load.index(5,4),"editable power"),"readonly/invalid accepted");
        check(load.setData(load.index(5,2),"12V/2A") && load.setData(load.index(5,3),"3"),"load values");
        check(load.setData(load.index(3,3),"10"),"voltage metadata");
        check(!load.data(load.index(5,4)).toString().isEmpty(),"power did not refresh");
        check(dynamic.setData(dynamic.index(3,2),"1~5") && dynamic.setData(dynamic.index(3,4),"10~20"),"dynamic values");
        const auto row=dynamic.record(0); int commits=0;
        QObject::connect(&vm,&TestConditionsViewModel::conditionsChanged,[&]{++commits;});
        check(dynamic.insertRecords(1,{row,row}) && commits==1,"paste not atomic");
        check(vm.dynamicMeta().t1t2.size()==3 && vm.dynamicMeta().t1t2[2]=="10~20","times detached from rows");
        auto invalid=row; invalid[1]="invalid"; commits=0;
        check(!dynamic.insertRecords(0,{row,invalid}) && commits==0 && dynamic.recordCount()==3,"partial invalid paste");
        dynamic.removeRecords({0,2});
        check(dynamic.recordCount()==1 && vm.dynamicMeta().t1t2.size()==1,"delete times");
        check(dc.setData(dc.index(0,5),"24") && dc.setData(dc.index(1,5),"2"),"hidden DC source lost");
        auto group=dc.record(0); group[9]="logic";
        check(dc.insertRecords(1,{group}),"DC group paste"); dc.removeRecords({0});
        check(vm.conditions().dcRows3[0].label=="logic" && vm.conditions().dcRows3[0].vin=="24","DC labels/values lost");
        QString xml; QXmlStreamWriter writer(&xml); vm.writeXml(writer);
        TestConditionsModel restored; QXmlStreamReader reader(xml); reader.readNextStartElement(); restored.loadXml(reader);
        check(!reader.hasError() && restored.snapshot().dynamicMeta.t1t2[0]=="10~20" && restored.snapshot().loadRows[0].values[0]=="12V/2A","XML roundtrip");
        std::cout<<"PASS grouped conditions\n";
    } catch(const std::exception &e) { std::cerr<<e.what(); return 1; }
}
