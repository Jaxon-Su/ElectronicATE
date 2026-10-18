#include "testsequencetaskkind.h"
#include "measurementresultmapping.h"
#include "testsequenceexcelreport.h"
#include "ziparchive.h"
#include "../../../src/service/report/testsequenceresultrows.h"
#include "../../../src/viewmodels/presentation/taskresultformatter.h"
#include <QCoreApplication>
#include <QDomDocument>
#include <QTemporaryDir>
#include <QFile>
#include <iostream>
#include <stdexcept>
void require(bool ok, const char *message) { if (!ok) throw std::runtime_error(message); }
int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    try {
        const QVector<MeasurementTrial> trials{
            {1,MeasurementDirection::Rise,1,1,1,{{1,"V",12,-3,6,5}}},
            {2,MeasurementDirection::Fall,2,1,-1,{{1,"V",8,-7,4,2}}}};
        QVector<TestSequenceResultRecord> records;
        int measurementKinds=0;
        for (const auto &descriptor : TestSequenceTaskKind::descriptors) {
            if (!descriptor.measurement) continue;
            ++measurementKinds;
            TestSequenceResultRecord record;
            record.taskName=QString::fromLatin1(descriptor.name); record.status="Pass";
            record.trials=trials; record.channels=representativeChannels(trials);
            if (TestSequenceTaskKind::isGroup(descriptor.kind)) {
                record.conditions={{{0,"First",LoadConditionKind::Static},"Pass",{},record.channels,trials},
                                   {{1,"Skipped",LoadConditionKind::Static},"Skipped",{},{},{}}};
                record.channels={{9,"",9999,-9999,9999,9999}};
                record.trials.clear();
            }
            const auto text=TaskResultFormatter::measurement(record,true);
            require(text.contains("2 trials") && !text.contains("Max") && !text.contains("9999"),"Tasks summary invalid");
            records.append(record);
            record.channels.clear();record.trials.clear();record.conditions.clear();record.status="Fail";
            records.append(record);
        }
        QTemporaryDir dir;QString path,error;
        require(TestSequenceExcelReport::save(dir.path(),"metrics",records,path,error),qPrintable(error));
        require(TestSequenceExcelReport::save(dir.path(),"metrics",records,path,error),"append failed");
        QFile file(path);require(file.open(QIODevice::ReadOnly),"missing report");
        QList<QPair<QByteArray,QByteArray>> parts;
        require(ZipArchive::readStored(file.readAll(),parts),"invalid ZIP");
        int sheets=0;
        for(const auto &part:parts) if(part.first.startsWith("xl/worksheets/")) {
            ++sheets;QDomDocument doc;require(bool(doc.setContent(part.second)),"invalid XML");
            const auto rows=doc.elementsByTagName("row");int quartets=0,missing=0;
            for(int i=1;i<rows.size();++i) {
                QMap<QChar,QString> cells;
                for(auto c=rows.at(i).firstChildElement("c");!c.isNull();c=c.nextSiblingElement("c")) cells[c.attribute("r").front()]=c.text();
                require(!cells['E'].contains("9999"),"Group aggregate leaked");
                if(cells['D']=="CH1") {
                    ++quartets;
                    require(cells['N']=="Rise"?cells['E']=="12"&&cells['F']=="-3"&&cells['G']=="6"&&cells['H']=="5"
                                              :cells['E']=="8"&&cells['F']=="-7"&&cells['G']=="4"&&cells['H']=="2","quartet mixed or suppressed");
                    require(cells['O']==(cells['N']=="Rise"?"1":"2"),"record identity lost");
                } else {++missing;require(cells['E']=="NA"||cells['E']=="Not run","missing event fabricated");}
            }
            require(quartets==measurementKinds*4 && missing>0,"mixed-strategy append lost rows");
        }
        require(sheets==2,"missing summary/Trial sheet");
        std::cout<<"PASS all measurement kinds, full quartets, failures, Groups and two-sheet append\n";
    } catch(const std::exception &e){std::cerr<<e.what();return 1;}
}
