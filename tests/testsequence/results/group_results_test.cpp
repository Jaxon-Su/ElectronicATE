#include "testsequencerunpanel.h"
#include "taskresultformatter.h"
#include "testsequenceexcelreport.h"
#include "ziparchive.h"
#include "../../../src/service/report/testsequenceresultrows.h"
#include <QApplication>
#include <QDomDocument>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char **argv)
{
    QApplication app(argc,argv);
    try {
        QVector<TestSequenceResultRecord> records;
        for(const auto &d:TestSequenceTaskKind::descriptors) {
            if(!TestSequenceTaskKind::isGroup(d.kind))continue;
            TestSequenceResultRecord r;r.taskName=QString::fromLatin1(d.name);r.status="Fail";r.attempt=2;r.error="member failed";
            TestSequenceChannelResult a{1,"V",12,-1,5,4,MeasurementDirection::Rise,1,1,1,0};
            TestSequenceChannelResult b{2,"A",3,-2,2,1,MeasurementDirection::Rise,1,1,1,0};
            TestSequenceChannelResult c{1,"V",9,-4,NAN,0,MeasurementDirection::Fall,1,1,1,0};
            r.channels={{9,"",9999,-9999,9999,9999}};
            r.conditions={{{0,"Same label",LoadConditionKind::Static},"Pass",{},{a,b},{}},
                          {{2,"Same label",LoadConditionKind::Static},"Fail","query failed",{c},{}},
                          {{3,"Not run",LoadConditionKind::Static},"Skipped",{},{},{}}};
            const auto text=TaskResultFormatter::measurement(r,false);
            require(text.contains("3 loads")&&text.contains("Incomplete")&&!text.contains("9999")&&!text.contains("RMS"),"Tasks not brief");
            const auto rows=TestSequenceResultRows::rows(r,true);
            require(rows.size()==7,"missing direction placeholders");
            require(rows[0][12].toString().contains("row 1 (Same label)")&&rows[3][12].toString().contains("row 3 (Same label)"),"duplicate labels mixed");
            require(rows[0][4].toDouble()==12&&rows[1][4].toDouble()==3&&rows[3][4].toDouble()==9,"per-load values mixed");
            require(rows[3][6].toString()=="NA"&&rows[3][7].toDouble()==0&&rows[3][8]=="Fail"&&rows[3][9].toString().contains("query failed"),"partial quartet/error lost");
            require(rows[5][8]=="Skipped"&&rows[5][4]=="NA","Skipped borrowed values");
            auto cancelled=r;cancelled.conditions[1].status="Cancelled";
            require(TestSequenceResultRows::rows(cancelled,true)[3][8]=="Cancelled","cancelled status lost");
            records.append(r);
        }
        QTemporaryDir dir;QString path,error;
        require(TestSequenceExcelReport::save(dir.path(),"groups",records,path,error),qPrintable(error));
        require(TestSequenceExcelReport::save(dir.path(),"groups",records,path,error),"append failed");
        QFile file(path);require(file.open(QIODevice::ReadOnly),"report missing");
        QList<QPair<QByteArray,QByteArray>> parts;require(ZipArchive::readStored(file.readAll(),parts),"invalid ZIP");
        for(const auto &p:parts)if(p.first=="xl/worksheets/sheet1.xml") {
            QDomDocument doc;require(bool(doc.setContent(p.second)),"invalid XML");
            require(doc.elementsByTagName("row").size()==85,"Group append rows missing");
            require(!p.second.contains("9999"),"stale group aggregate exported");
        }
        TestSequenceRunPanel panel;panel.resize(800,400);panel.setActiveTasks({{1,records[0].taskName}});
        const auto text=TaskResultFormatter::measurement(records[0],false);panel.setTaskResult(0,text);
        auto *table=panel.findChild<QTableWidget*>();
        require(table->rowHeight(0)==48&&table->item(0,4)->toolTip()==text,"summary row grew or tooltip lost");
        std::cout<<"PASS six Groups, partial/Skipped/cancelled results, duplicate labels, append, compact Tasks\n";
    }catch(const std::exception &e){std::cerr<<e.what();return 1;}
}
