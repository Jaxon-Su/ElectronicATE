#include "../../support/transient_test_support.h"
#include "steadystatesearchstrategy.h"
#include "scopemeasurementstrategy.h"
#include "turnoffstrategy.h"
#include "measurementresultmapping.h"
#include "testsequenceexcelreport.h"
#include "../../../src/service/report/testsequenceresultrows.h"
#include "../../../src/viewmodels/testsequence/testsequenceresultsmodel.h"
#include "../../../src/viewmodels/presentation/taskresultformatter.h"
#include "../../../src/views/testsequence/testsequenceresultdetails.h"
#include "../../../src/views/testsequence/testsequencerunpanel.h"
#include "ziparchive.h"
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QSignalSpy>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QVBoxLayout>
#include <QLockFile>
#include <QFontDatabase>
#include <QThread>
#include <QXmlStreamReader>

// Completed AUTO, two pre-event records, then optional real events and a MISS.
// Values are deliberately distinct across acquisitions and channels.
class WorstReferenceScope : public Scope {
public:
    int acquisition = 0;
    bool realEvents = false;
    QString fault, readError;
    QAtomicInt *stopFlag = nullptr;
    bool beginSingleAcquisition() override {
        if (autoMode) acquisition=0; else ++acquisition;
        return Scope::beginSingleAcquisition();
    }
    QString getAcquisitionState() override {
        return autoMode || (!event && acquisition<=2) || (event && realEvents && acquisition<=4) ? "0" : "1";
    }
    bool isClipping(int ch) override {
        return fault=="clipped" && !autoMode && !event && acquisition==1 && ch==2;
    }
    QString lastError() const override { return readError; }
    double measureSignalPeak(int ch,const QString &type) override {
        readError.clear();
        const int index=autoMode ? 0 : qMin(acquisition,4);
        const bool baseline=!autoMode && !event;
        if (baseline && ch==2) {
            if ((fault=="query" || (fault=="second-query" && negative)) && type=="RMS") {
                readError="Baseline RMS query failed";return NAN;
            }
            if (fault=="stop" && type=="RMS") stopFlag->storeRelease(1);
            if (fault=="timeout" && type=="RMS") QThread::msleep(50);
            if (fault=="rank-na" && (type=="MAXimum" || type=="MINimum")) return NAN;
        }
        if (baseline && fault=="rms-na" && type=="RMS") return NAN;
        if (baseline && fault=="other-na" && type==(negative?"MAXimum":"MINimum")) return NAN;
        const double extrema[2][5]={{20,24,26,30,35},{30,40,35,25,24}};
        const double lows[2][5]={{20,24,26,30,35},{30,32,45,23,22}};
        double peak=extrema[ch-1][index];
        double low=lows[ch-1][index];
        if (baseline && fault=="tie" && index==2 && ch==1) peak=low=24;
        if (baseline && fault=="clipped" && index==1) peak=low=1000;
        if (type=="MAXimum") return peak;
        if (type=="MINimum") return -low;
        if (type=="RMS") return 10+index+ch;
        return 5+index+ch;
    }
};

class ValuesScope : public Scope {
public:
    int failMetric = -1;
    bool requireSafeExit = false;
    double measureSignalPeak(int channel, const QString &type) override {
        if (event && !autoMode) {
            if (requireSafeExit && (type == "RMS" || type == "MEAN")) {
                require(trace.lastIndexOf("POWER OFF") > trace.lastIndexOf("POWER ON"), "RMS/Mean read before short Input OFF");
                require(trace.lastIndexOf("RELAY 0 0") > trace.lastIndexOf("RELAY 1 0"), "RMS/Mean read before short release");
            }
            if ((failMetric == 0 && type == "RMS") || (failMetric == 1 && type == "MEAN"))
                throw std::runtime_error("trial measurement rejected");
            if (type == "RMS") return events * 10 + channel;
            if (type == "MEAN") return -(events * 10 + channel);
        }
        return Scope::measureSignalPeak(channel, type);
    }
};

class ReferenceScope : public ValuesScope {
public:
    int references = 0;
    QString fault, readError;
    QAtomicInt *stop = nullptr;
    bool beginSingleAcquisition() override {
        if (autoMode) ++references;
        else if (fault == "stop-search") stop->storeRelease(1);
        return Scope::beginSingleAcquisition();
    }
    QString lastError() const override { return readError; }
    double measureSignalPeak(int channel, const QString &type) override {
        readError.clear();
        if (!autoMode) {
            const double value = ValuesScope::measureSignalPeak(channel, type);
            return event && fault == "event-rms-na" && type == "RMS" ? NAN : value;
        }
        if (type == "RMS" || type == "MEAN") {
            if ((fault == "second-error" && references == 2) ||
                (fault == "secondary-error" && channel == 2)) {
                readError = "reference query failed"; return NAN;
            }
            if (fault == type + "-error") { readError = "reference query failed"; return NAN; }
            if (fault == type + "-na") return NAN;
            if (fault == "stop-reference") stop->storeRelease(1);
            if (fault == "timeout") QThread::msleep(30);
        }
        // Distinct synthetic acquisitions; event metrics are independently generated by ValuesScope.
        if (channel == 1) {
            if (type == "MAXimum") return references == 1 ? 1 : 1.1;
            if (type == "MINimum") return references == 1 ? .8 : .9;
            if (type == "RMS") return references == 1 ? .91 : 1.01;
            return references == 1 ? .9 : 1;
        }
        if (type == "MAXimum") return references == 1 ? 12 : 22;
        if (type == "MINimum") return references == 1 ? 8 : 18;
        if (type == "RMS") return references == 1 ? 10.5 : 20.5;
        return references == 1 ? 10 : 20;
    }
};

class TrialResultsTest : public QObject {
    Q_OBJECT
    static QVector<MeasurementTrial> trials()
    {
        return {
            {1, MeasurementDirection::Rise, 1, 1, 1, {{1,"V",6,-1,3,2},{2,"A",8,-2,4,3}}},
            {2, MeasurementDirection::Rise, 1, 2, 1, {{1,"V",7,-.5,5,4},{2,"A",7,-9,6,5}}},
            {3, MeasurementDirection::Rise, 1, 3, 1, {{1,"V",7,-5,8,7},{2,"A",NAN,-8,NAN,6}}},
            {4, MeasurementDirection::Fall, 1, 1, -1, {{1,"V",9,-3,2,1},{2,"A",4,-4,2,-1}}},
            {5, MeasurementDirection::Fall, 2, 1, -2, {{1,"V",3,-6,NAN,-2},{2,"A",20,-2,1,0}}}
        };
    }
    static TestSequenceResultRecord record(int attempt = 1)
    {
        TestSequenceResultRecord r;
        r.runId="fixture-run"; r.taskIndex=0; r.dutUid=42; r.attempt=attempt;
        r.taskName="Turn on"; r.externalName="=literal & 中文"; r.status="Pass";
        r.finishedAt=QDateTime::currentDateTimeUtc(); r.trials=trials();
        r.channels=representativeChannels(r.trials);
        return r;
    }
private slots:
    void descentDetailDefault()
    {
        auto r=record(); r.taskName="Turn off (descent)"; r.trials.clear();
        for (int i=0;i<5;++i)
            r.trials.append({i+1,MeasurementDirection::Fall,i+1,1,10.0-i,
                             {{1,"V",20.0+i,-1.0-i,10.0+i,5.0+i}}});
        r.channels=representativeChannels(r.trials);
        TestSequenceResultsModel model; TestSequenceResultDetails view(&model);
        model.selectTask(r.dutUid); model.updateRecord(r);
        auto *mode=view.findChild<QComboBox *>("resultDetailMode");
        QCOMPARE(mode->currentText(),"Trials");
        QCOMPARE(model.rowCount(),5);
        for(int i=0;i<5;++i) {
            QCOMPARE(model.data(model.index(i,TestSequenceResultRows::Level)).toDouble(),10.0-i);
            QCOMPARE(model.data(model.index(i,TestSequenceResultRows::Max)).toDouble(),20.0+i);
            QCOMPARE(model.data(model.index(i,TestSequenceResultRows::Min)).toDouble(),-1.0-i);
            QCOMPARE(model.data(model.index(i,TestSequenceResultRows::Rms)).toDouble(),10.0+i);
            QCOMPARE(model.data(model.index(i,TestSequenceResultRows::Mean)).toDouble(),5.0+i);
        }
        mode->setCurrentIndex(0); QCOMPARE(model.rowCount(),2);
        model.updateRecord(r); QCOMPARE(mode->currentText(),"Summary");
        auto other=record(); other.dutUid=51;other.taskIndex=1;model.updateRecord(other);
        model.selectTask(other.dutUid);QCOMPARE(mode->currentText(),"Summary");
        model.selectTask(r.dutUid);QCOMPARE(mode->currentText(),"Summary");
        mode->setCurrentIndex(1);QCOMPARE(model.rowCount(),5);
    }
    void descentTrialIssues()
    {
        auto r=record();r.taskName="Turn off (descent)";r.status="Fail";r.error="one pre-power-off trigger";
        r.trials={{1,MeasurementDirection::Fall,1,1,10,{{1,"V",20,-1,10,5}}},
                  {2,MeasurementDirection::Fall,3,1,8,{{1,"V",22,-3,12,7}}}};
        r.trialIssues={{MeasurementDirection::Fall,2,1,9,false,"Pre-power-off trigger"},
                       {MeasurementDirection::Fall,2,2,9,true,"Skipped after early trigger"}};
        r.channels=representativeChannels(r.trials);
        TestSequenceResultsModel model;model.updateRecord(r);model.selectTask(r.dutUid);
        TestSequenceResultDetails view(&model);
        QCOMPARE(view.findChild<QComboBox *>("resultDetailMode")->currentText(),"Trials");
        QCOMPARE(model.rowCount(),4);
        const QStringList expected{"Pass","Fail","Skipped","Pass"};
        for(int i=0;i<4;++i)QCOMPARE(model.data(model.index(i,TestSequenceResultRows::Status)).toString(),expected[i]);
        QCOMPARE(model.data(model.index(1,TestSequenceResultRows::Record)).toString(),"—");
        QCOMPARE(model.data(model.index(1,TestSequenceResultRows::Max)).toString(),"NA");
        QCOMPARE(model.data(model.index(1,TestSequenceResultRows::Level)).toDouble(),9.0);
        auto *mode=view.findChild<QComboBox *>("resultDetailMode");mode->setCurrentIndex(0);
        QCOMPARE(model.rowCount(),4); // Fall representative, Rise not run, Fail and Skipped.
        const auto rows=TestSequenceResultRows::rows(r,true);
        QCOMPARE(rows[0][TestSequenceResultRows::Step].toInt(),3);
        QCOMPARE(rows[0][TestSequenceResultRows::Min].toDouble(),-3.0);
        mode->setCurrentIndex(1);
        model.setFilter(TestSequenceResultsModel::ChannelFilter,"CH1");QCOMPARE(model.rowCount(),2);
        model.setFilter(TestSequenceResultsModel::ChannelFilter,{});QCOMPARE(model.rowCount(),4);
        auto retry=r;retry.attempt=2;model.updateRecord(retry);QCOMPARE(model.rowCount(),8);
        model.setFilter(TestSequenceResultsModel::AttemptFilter,"2");QCOMPARE(model.rowCount(),4);
        auto empty=r;empty.dutUid=50;empty.taskIndex=1;empty.trials[0].channels.clear();empty.trials[1].channels.clear();empty.channels.clear();
        model.updateRecord(empty);model.selectTask(empty.dutUid);QCOMPARE(model.rowCount(),4);
        QCOMPARE(model.data(model.index(0,TestSequenceResultRows::Level)).toDouble(),10.0);
        QCOMPARE(model.data(model.index(0,TestSequenceResultRows::Max)).toString(),"NA");
    }
    void turnoffWorstReference_data()
    {
        QTest::addColumn<QString>("target");
        QTest::addColumn<QString>("fault");
        for (const QString &target:{"MAX","MIN","BOTH"})
            for (const QString &fault:{"none","tie","rank-na","rms-na","other-na","clipped","events"})
                QTest::newRow(qPrintable(target+"-"+fault))<<target<<fault;
    }
    void turnoffWorstReference()
    {
        QFETCH(QString,target); QFETCH(QString,fault);
        WorstReferenceScope scope; scope.scale=25; scope.fault=fault; scope.realEvents=fault=="events";
        QAtomicInt stop(0); auto ctx=context(scope);
        ctx.target=target; ctx.trialsPerLevel=1; ctx.timeoutMs=20;
        QStringList logs; ctx.log=[&](const QString &message){logs<<message;};
        ctx.captureRecord=[](const ScopeCaptureRecord &record) {
            for (const auto &value:record.channels)
                require(std::isnan(value.rms) && std::isnan(value.mean),"Capture quartet contract changed");
        };
        // Capture itself requires finite extrema; exercise legal ranking NA on
        // the result-only path, without changing that separate capture contract.
        if(fault=="rank-na" || fault=="other-na")ctx.captureRecord={};
        std::unique_ptr<IOscilloscopeMeasureStrategy> algorithm(OscilloscopeStrategyFactory::create("Turn off",&ctx));
        algorithm->setCaptureObserver(ctx.captureRecord);
        ScopeMeasurementStrategy strategy(std::move(algorithm),scope);
        const auto result=strategy.execute(stop);
        QVERIFY2(result.success,qPrintable(result.error));
        QCOMPARE(result.trials.isEmpty(),!scope.realEvents);
        const auto selected=representativeChannels(result.trials,result.steadyReferences);
        QCOMPARE(selected.size(),target=="BOTH"?4:2);
        for (const auto &value:selected) {
            const bool first=value.channel==1;
            const bool rise=value.direction==MeasurementDirection::Rise;
            const double peak=scope.realEvents ? (first?35:25) : first ? (fault=="tie"?24:26)
                : fault=="rank-na"?30 : fault=="clipped" || !rise?35 : 40;
            const double low=first ? -peak : scope.realEvents?-23 : fault=="rank-na"?-30
                : fault=="clipped" || !rise?-45:-32;
            const int index=scope.realEvents ? (first?4:3) : first ? (fault=="tie"?1:2)
                : fault=="rank-na"?0 : fault=="clipped" || !rise?2 : 1;
            if(fault=="other-na" && !rise)QVERIFY(std::isnan(value.maximum));else QCOMPARE(value.maximum,peak);
            if(fault=="other-na" && rise)QVERIFY(std::isnan(value.minimum));else QCOMPARE(value.minimum,low);
            if (fault=="rms-na") QVERIFY(std::isnan(value.rms));
            else QCOMPARE(value.rms,double(10+index+value.channel));
            QCOMPARE(value.mean,double(5+index+value.channel));
            QCOMPARE(value.source,scope.realEvents?MeasurementSource::Event:MeasurementSource::SteadyReference);
            QCOMPARE(value.sourceSequence>0,scope.realEvents);
        }
        QVERIFY(logs.join('\n').contains("Baseline reference updated"));
        if (scope.realEvents) QVERIFY(result.steadyReferences.isEmpty());
        // The actual adapter output feeds both Detail modes and XLSX, with each
        // Group/load carrying independent values; an unrelated attempt stays separate.
        if (fault=="none" && target=="BOTH") {
            auto single=record(); single.taskName="Turn off"; single.channels=selected; single.trials.clear();
            auto group=single; group.taskName="Turn off Group Test"; group.dutUid=43;group.taskIndex=1;
            group.conditions={{{0,"Light",LoadConditionKind::Static},"Pass",{},selected,{}},
                              {{1,"Full",LoadConditionKind::Static},"Pass",{},selected,{}}};
            group.channels.clear();
            TestSequenceResultsModel model;model.updateRecord(single);model.updateRecord(group);
            for (int uid:{42,43}) for (bool summary:{true,false}) {
                model.selectTask(uid);model.setSummary(summary);
                QCOMPARE(model.rowCount(),uid==42?4:8);
                for(int i=0;i<model.rowCount();++i) {
                    const bool ch1=model.data(model.index(i,TestSequenceResultRows::Channel)).toString()=="CH1";
                    const bool rise=model.data(model.index(i,TestSequenceResultRows::Direction)).toString()=="Rise";
                    QCOMPARE(model.data(model.index(i,TestSequenceResultRows::Max)).toDouble(),ch1?26.:rise?40.:35.);
                    QCOMPARE(model.data(model.index(i,TestSequenceResultRows::Min)).toDouble(),ch1?-26.:rise?-32.:-45.);
                    QCOMPARE(model.data(model.index(i,TestSequenceResultRows::Rms)).toDouble(),ch1||rise?13.:14.);
                    QCOMPARE(model.data(model.index(i,TestSequenceResultRows::Mean)).toDouble(),ch1||rise?8.:9.);
                }
            }
            QTemporaryDir dir;QString path,error;
            QVERIFY2(TestSequenceExcelReport::save(dir.path(),"worst-reference",{single,group},path,error),qPrintable(error));
            QFile file(path);QVERIFY(file.open(QIODevice::ReadOnly));auto bytes=file.readAll();
            QList<QPair<QByteArray,QByteArray>> parts;QVERIFY(ZipArchive::readStored(bytes,parts));
            int sheets=0;
            for(const auto &part:parts) if(part.first.startsWith("xl/worksheets/sheet")) {
                ++sheets;QXmlStreamReader xml(part.second);
                QVector<QMap<QChar,QString>> rows;QChar column;
                while(!xml.atEnd()) {
                    xml.readNext();if(!xml.isStartElement())continue;
                    if(xml.name()==u"row")rows.append(QMap<QChar,QString>{});
                    else if(xml.name()==u"c")column=xml.attributes().value("r").front();
                    else if(xml.name()==u"v" || xml.name()==u"t")rows.last()[column]=xml.readElementText();
                }
                QVERIFY(!xml.hasError());QCOMPARE(rows.size(),13);
                for(int i=1;i<rows.size();++i) {
                    const auto cells=rows[i];
                    const bool ch1=cells['D']=="CH1";
                    const bool rise=cells['N']=="Rise";
                    QCOMPARE(cells['E'].toDouble(),ch1?26.:rise?40.:35.);QCOMPARE(cells['F'].toDouble(),ch1?-26.:rise?-32.:-45.);
                    QCOMPARE(cells['G'].toDouble(),ch1||rise?13.:14.);QCOMPARE(cells['H'].toDouble(),ch1||rise?8.:9.);
                    QVERIFY(cells['O'].isEmpty());QVERIFY(cells['J'].contains("Steady reference"));
                }
            }
            QCOMPARE(sheets,2);
            if(!qEnvironmentVariable("TRIAL_REVIEW_DIR").isEmpty()) {
                QFile output(QDir(qEnvironmentVariable("TRIAL_REVIEW_DIR")).filePath("worst-reference.xlsx"));
                QVERIFY(output.open(QIODevice::WriteOnly));output.write(bytes);
            }
        }
    }
    void turnoffBaselineFailures_data()
    {
        QTest::addColumn<QString>("fault");
        for (const QString &fault:{"query","second-query","stop","timeout","capture"}) QTest::newRow(qPrintable(fault))<<fault;
    }
    void turnoffBaselineFailures()
    {
        QFETCH(QString,fault);
        WorstReferenceScope scope;scope.scale=25;scope.fault=fault;
        QAtomicInt stop(0);scope.stopFlag=&stop;auto ctx=context(scope);
        ctx.target="MAX";ctx.timeoutMs=20;
        if(fault=="second-query")ctx.target="BOTH";
        if(fault=="timeout")ctx.phaseTimeoutMs=40;
        ctx.captureRecord=[&](const ScopeCaptureRecord &record) {
            if(fault=="capture" && record.phase=="Baseline")throw std::runtime_error("Baseline capture failed");
        };
        const auto result=runTransientStrategy(TransientKind::TurnOff,ctx,&scope,stop);
        QVERIFY(!result.success);QVERIFY(result.trials.isEmpty());
        QCOMPARE(result.steadyReferences.size(),fault=="second-query"?1:0);
        if(fault=="second-query")QCOMPARE(result.steadyReferences[0].direction,MeasurementDirection::Rise);
        QVERIFY2(result.errorMessage.contains(fault.endsWith("query")?"RMS query failed":fault=="stop"?"Stopped":
                                              fault=="timeout"?"time limit":"capture failed"),qPrintable(result.errorMessage));
        QVERIFY(scope.trace.contains("ACQuire:STATE STOP"));QVERIFY(scope.trace.contains("POWER OFF"));
        QVERIFY(scope.trace.contains("RELAY 0 0"));QVERIFY(scope.trace.contains("LOAD OFF"));
    }
    void singleLoadDetails()
    {
        auto single = record();
        single.loadCondition = {2, "Full Load", LoadConditionKind::Static};
        auto dynamic = record();
        dynamic.taskIndex = 1; dynamic.dutUid = 43; dynamic.taskName = "Dynamic Test";
        dynamic.loadCondition = {1, "Step Load", LoadConditionKind::Dynamic};
        const QString staticLoad = "Load row 3 (Full Load)";
        const QString dynamicLoad = "Dy Load row 2 (Step Load)";
        TestSequenceResultsModel model;
        model.updateRecord(single); model.updateRecord(dynamic); model.selectTask(42);
        TestSequenceResultDetails details(&model);
        auto *filter = details.findChild<QComboBox *>("resultFilter12");
        for (bool summary : {true, false}) {
            model.setSummary(summary);
            QCOMPARE(model.options(TestSequenceResultsModel::LoadFilter), QStringList{staticLoad});
            QCOMPARE(filter->count(), 2);
            filter->setCurrentIndex(filter->findData(staticLoad));
            QCOMPARE(model.rowCount(), summary ? 4 : 10);
            for (int i = 0; i < model.rowCount(); ++i)
                QCOMPARE(model.data(model.index(i, TestSequenceResultRows::Load)).toString(), staticLoad);
            auto failed = single; failed.attempt = 2; failed.status = "Fail";
            failed.trials.clear(); failed.channels.clear();
            for (const auto &row : TestSequenceResultRows::rows(failed, summary))
                QCOMPARE(row[TestSequenceResultRows::Load].toString(), staticLoad);
        }
        model.selectTask(43);
        QCOMPARE(filter->currentIndex(), 0);
        QCOMPARE(model.options(TestSequenceResultsModel::LoadFilter), QStringList{dynamicLoad});
        auto absent = single; absent.loadCondition = {};
        QVERIFY(TestSequenceResultRows::rows(absent, true).first()[TestSequenceResultRows::Load].toString().isEmpty());
        auto delay = single; delay.taskName = "Delay";
        QVERIFY(TestSequenceResultRows::rows(delay, true).first()[TestSequenceResultRows::Load].toString().isEmpty());
        QCOMPARE(TestSequenceResultRows::loadDescription({0, {}, LoadConditionKind::Static}), "Load row 1");

        QTemporaryDir dir; QString path, error;
        QVERIFY2(TestSequenceExcelReport::save(dir.path(), "single-load", {single, dynamic}, path, error), qPrintable(error));
        QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); const auto bytes = file.readAll();
        QList<QPair<QByteArray, QByteArray>> parts; QVERIFY(ZipArchive::readStored(bytes, parts));
        int sheets = 0;
        for (const auto &part : parts)
            if (part.first.startsWith("xl/worksheets/sheet")) {
                ++sheets; QVERIFY(part.second.contains(staticLoad.toUtf8()));
                QVERIFY(part.second.contains(dynamicLoad.toUtf8()));
            }
        QCOMPARE(sheets, 2);
        const auto evidence = qEnvironmentVariable("TRIAL_REVIEW_DIR");
        if (!evidence.isEmpty()) {
            QFile output(QDir(evidence).filePath("single-load-results.xlsx"));
            QVERIFY(output.open(QIODevice::WriteOnly)); output.write(bytes);
        }
    }
    void initTestCase()
    {
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
        QFontDatabase::addApplicationFont("C:/Windows/Fonts/msjh.ttc");
        QApplication::setFont(QFont("Microsoft JhengHei",10));
    }
    void representatives()
    {
        const auto result = representativeChannels(trials());
        QCOMPARE(result.size(), 4);
        QCOMPARE(result[0].sourceSequence, 2); QCOMPARE(result[0].minimum, -.5);
        QCOMPARE(result[0].rms, 5.); QCOMPARE(result[0].mean, 4.);
        QCOMPARE(result[1].sourceSequence, 1); QCOMPARE(result[1].maximum, 8.);
        QCOMPARE(result[2].sourceSequence, 5); QCOMPARE(result[2].maximum, 3.);
        QVERIFY(std::isnan(result[2].rms)); QCOMPARE(result[2].mean, -2.);
        QCOMPARE(result[3].sourceSequence, 4); QCOMPARE(result[3].maximum, 4.);
        auto invalid=trials(); for(auto &t:invalid) for(auto &v:t.channels) {v.maximum=NAN;v.minimum=NAN;}
        QVERIFY(representativeChannels(invalid).isEmpty());
        const QVector<MeasurementReference> fallback{{MeasurementDirection::Rise, {{1,"V",100,-100,99,98}}},
                                                     {MeasurementDirection::Fall, {{1,"V",100,-100,99,98}}}};
        QVERIFY(representativeChannels(invalid, fallback).isEmpty()); // NA event ranks do not become references.
        const auto withReference = representativeChannels(trials(), fallback);
        QCOMPARE(withReference[2].sourceSequence, 5);
        QVERIFY(std::isnan(withReference[2].rms)); // Never fill a missing event metric from AUTO.
        auto r=record();r.trials=invalid;r.channels=representativeChannels(invalid);
        auto rows=TestSequenceResultRows::rows(r,true);QCOMPARE(rows.size(),4);
        for(const auto &row:rows) {
            QVERIFY(!row[TestSequenceResultRows::Channel].toString().isEmpty());
            QCOMPARE(row[TestSequenceResultRows::Max].toString(),"NA");
            QVERIFY(!row[TestSequenceResultRows::Record].isValid());
        }
    }
    void transient_data()
    {
        QTest::addColumn<int>("kind"); QTest::addColumn<QString>("target");
        for (int kind=0; kind<4; ++kind) for (const QString &target : {"MAX","MIN","BOTH"})
            QTest::newRow(qPrintable(QString("%1-%2").arg(kind).arg(target))) << kind << target;
    }
    void transient()
    {
        QFETCH(int,kind); QFETCH(QString,target);
        ValuesScope scope; scope.single=TransientKind(kind)==TransientKind::TurnOn ||
                                         TransientKind(kind)==TransientKind::ShortThenTurnOn;
        scope.requireSafeExit=kind>=2;
        auto ctx=context(scope); ctx.target=target; ctx.trialsPerLevel=3;
        QAtomicInt stop(0);
        const auto result=runTransientStrategy(TransientKind(kind),ctx,&scope,stop);
        QVERIFY2(result.success,qPrintable(result.errorMessage)); QVERIFY(!result.trials.isEmpty());
        int sequence=0;
        for(const auto &trial:result.trials) {
            QCOMPARE(trial.sequence,++sequence); QVERIFY(trial.step>0 && trial.trial>=1 && trial.trial<=3);
            QVERIFY(std::isfinite(trial.triggerLevel)); QCOMPARE(trial.channels.size(),2);
            if(target=="MAX") QCOMPARE(trial.direction,MeasurementDirection::Rise);
            if(target=="MIN") QCOMPARE(trial.direction,MeasurementDirection::Fall);
            for(const auto &v:trial.channels) {
                QVERIFY(std::isfinite(v.maximum) && std::isfinite(v.minimum) && std::isfinite(v.rms));
                QCOMPARE(v.mean,-v.rms); QVERIFY(v.rms>10); // excludes AUTO/Baseline
            }
        }
        QCOMPARE(representativeChannels(result.trials).size(), target=="BOTH"?4:2);
    }
    void steady_data() { QTest::addColumn<QString>("target"); for(const QString &t:{"MAX","MIN","BOTH"}) QTest::newRow(qPrintable(t))<<t; }
    void steady()
    {
        QFETCH(QString,target);
        ValuesScope scope; scope.single=true;
        scope.onArm=[&]{if(!scope.autoMode){scope.event=true;++scope.events;}};
        QAtomicInt stop(0);
        const auto result=SteadyStateSearchStrategy(1,10000,100,target).execute(&scope,stop);
        QVERIFY2(result.success,qPrintable(result.errorMessage)); QVERIFY(!result.trials.isEmpty());
        for(const auto &trial:result.trials) for(const auto &v:trial.channels) {
            QVERIFY(std::isfinite(v.maximum) && std::isfinite(v.minimum));
            QVERIFY(v.rms>10); QCOMPARE(v.mean,-v.rms);
        }
        QCOMPARE(representativeChannels(result.trials).size(), target=="BOTH"?4:2);
    }
    void measurementFailures()
    {
        for(int metric:{0,1}) {
            ValuesScope scope; scope.single=true; scope.failMetric=metric;
            scope.requireSafeExit=true;
            auto ctx=context(scope); QAtomicInt stop(0);
            auto result=runTransientStrategy(TransientKind::ShortThenTurnOn,ctx,&scope,stop);
            QVERIFY(!result.success); QVERIFY(result.errorMessage.contains("trial measurement rejected"));
            QVERIFY(result.trials.isEmpty()); QVERIFY(scope.trace.contains("POWER OFF"));
            QVERIFY(scope.trace.contains("RELAY 0 0")); QVERIFY(scope.trace.contains("LOAD OFF"));
        }
    }
    void noEventIsNotReference()
    {
        Scope scope; scope.never=true; scope.single=false; QAtomicInt stop(0); auto ctx=context(scope);
        ctx.target = "MAX";
        auto result=runTransientStrategy(TransientKind::TurnOff,ctx,&scope,stop);
        QVERIFY(result.success); QVERIFY(result.trials.isEmpty());
        auto r=record(); r.taskName="Turn off"; r.trials=result.trials;
        r.channels=representativeChannels(result.trials, result.steadyReferences); r.settings["measure_target"]="MAX";
        auto rows=TestSequenceResultRows::rows(r,true);
        QCOMPARE(rows[0][TestSequenceResultRows::Max], QVariant(1.));
        QCOMPARE(rows[0][TestSequenceResultRows::Min], QVariant(1.));
        QCOMPARE(rows[0][TestSequenceResultRows::Rms], QVariant(1.));
        QCOMPARE(rows[0][TestSequenceResultRows::Mean], QVariant(1.));
        const auto summary = TaskResultFormatter::measurement(r, true);
        QVERIFY(summary.contains("1 directions") && summary.contains("2 CH") && summary.contains("0 trials"));
    }
    void transientReferenceResults_data()
    {
        QTest::addColumn<QString>("task");
        QTest::addColumn<QString>("target");
        QTest::addColumn<QString>("events");
        for (const QString &task : {"Turn off", "Turn on then short"})
            for (const QString &target : {"MAX", "MIN", "BOTH"})
                for (const QString &events : {"none", "rise", "fall", "both", "both-na"})
                    QTest::newRow(qPrintable(task + "-" + target + "-" + events)) << task << target << events;
    }
    void transientReferenceResults()
    {
        QFETCH(QString, task); QFETCH(QString, target); QFETCH(QString, events);
        ReferenceScope scope; scope.missMax = events == "none" || events == "fall";
        scope.missMin = events == "none" || events == "rise";
        // Keep the final-MISS retention fixture within one grid interval of the
        // first HIT; integer bisection may otherwise converge on a final HIT.
        if(events.startsWith("both")){scope.scale=5;scope.peak=1.3;scope.negativePeak=target=="MIN"?.5:.7;}
        scope.requireSafeExit = task == "Turn on then short";
        if (events == "both-na") scope.fault = "event-rms-na";
        QAtomicInt stop(0); auto ctx = context(scope); ctx.target = target;
        // This matrix verifies result provenance, not a 1 ms scheduler deadline.
        ctx.timeoutMs=50;
        QString lastOutcome;
        ctx.log = [&](const QString &message) {
            if (message.startsWith("Level:")) lastOutcome = message;
        };
        int archives = 0;
        ctx.captureRecord = [&](const ScopeCaptureRecord &record) {
            ++archives;
            for (const auto &value : record.channels) {
                require(std::isnan(value.rms) && std::isnan(value.mean), "Capture quartet contract changed");
            }
        };
        std::unique_ptr<IOscilloscopeMeasureStrategy> algorithm(OscilloscopeStrategyFactory::create(task, &ctx));
        algorithm->setCaptureObserver(ctx.captureRecord);
        ScopeMeasurementStrategy strategy(std::move(algorithm), scope);
        const auto result = strategy.execute(stop);
        QVERIFY2(result.success, qPrintable(result.error)); QVERIFY(archives > 0);
        auto r = record(); r.taskName = task; r.settings["measure_target"] = target;
        r.trials = result.trials; r.channels = representativeChannels(result.trials, result.steadyReferences);
        r.loadCondition = {2, "Full Load", LoadConditionKind::Static};
        int expectedReferences = 0;
        for (const auto direction : {MeasurementDirection::Rise, MeasurementDirection::Fall}) {
            const bool rise = direction == MeasurementDirection::Rise;
            if ((rise && target == "MIN") || (!rise && target == "MAX")) continue;
            const bool event = events.startsWith("both") || events == (rise ? "rise" : "fall");
            if (!event) ++expectedReferences;
            int channels = 0;
            for (const auto &ch : r.channels) {
                if (ch.direction != direction) continue;
                ++channels;
                QCOMPARE(ch.source, event ? MeasurementSource::Event : MeasurementSource::SteadyReference);
                if (event) {
                    QVERIFY(ch.sourceSequence > 0);
                    if (events == "both-na") { QVERIFY(std::isnan(ch.rms)); QVERIFY(ch.mean < -10); }
                    else { QVERIFY(ch.rms > 10); QCOMPARE(ch.mean, -ch.rms); }
                } else {
                    QCOMPARE(ch.sourceSequence, 0); QVERIFY(std::isnan(ch.triggerLevel));
                    const bool second = !rise && target == "BOTH";
                    if (ch.channel == 1) {
                        QCOMPARE(ch.maximum, second ? 1.1 : 1.); QCOMPARE(ch.minimum, second ? .9 : .8);
                        QCOMPARE(ch.rms, second ? 1.01 : .91); QCOMPARE(ch.mean, second ? 1. : .9);
                    } else {
                        QCOMPARE(ch.maximum, second ? 22. : 12.); QCOMPARE(ch.minimum, second ? 18. : 8.);
                        QCOMPARE(ch.rms, second ? 20.5 : 10.5); QCOMPARE(ch.mean, second ? 20. : 10.);
                    }
                }
            }
            QCOMPARE(channels, 2);
        }
        QCOMPARE(result.steadyReferences.size(), expectedReferences);
        if (events.startsWith("both")) QCOMPARE(lastOutcome, "Level: all trials missed");
        if (events == "none") QVERIFY(result.trials.isEmpty());
        TestSequenceResultsModel model; model.updateRecord(r); model.selectTask(r.dutUid);
        for (bool summary : {true, false}) {
            model.setSummary(summary);
            const auto rows = TestSequenceResultRows::rows(r, summary);
            QCOMPARE(model.rowCount(), rows.size());
            int referenceRows = 0;
            for (int index = 0; index < rows.size(); ++index) {
                const auto &row = rows[index];
                QCOMPARE(row[TestSequenceResultRows::Load].toString(), "Load row 3 (Full Load)");
                if (!row[TestSequenceResultRows::Note].toString().contains("Steady reference")) continue;
                ++referenceRows;
                QVERIFY(row[TestSequenceResultRows::Note].toString().startsWith("Steady reference"));
                QVERIFY(!row[TestSequenceResultRows::Record].isValid());
                QVERIFY(!row[TestSequenceResultRows::Step].isValid());
                QVERIFY(!row[TestSequenceResultRows::Trial].isValid());
                QVERIFY(!row[TestSequenceResultRows::Level].isValid());
                QVERIFY(model.data(model.index(index, TestSequenceResultRows::Max), Qt::ToolTipRole)
                            .toString().contains("Steady reference"));
            }
            QCOMPARE(referenceRows, expectedReferences * 2);
            if (summary && target != "BOTH")
                QCOMPARE(rows.last()[TestSequenceResultRows::Max].toString(), "Not run");
        }
        if (target == "BOTH" && events == "none") {
            auto group = r; group.taskName = task + " Group Test"; group.dutUid = 43;
            group.channels.clear(); group.trials.clear();
            group.conditions = {{{0, "Light", LoadConditionKind::Static}, "Pass", {}, r.channels, {}},
                                {{2, "Full", LoadConditionKind::Static}, "Pass", {}, r.channels, {}}};
            const auto taskSummary = TaskResultFormatter::measurement(group, true);
            QVERIFY(taskSummary.contains("2 directions") && taskSummary.contains("2 CH") &&
                    taskSummary.contains("0 trials") && taskSummary.contains("2 loads"));
            auto retry = r; retry.attempt = 2; retry.status = "Fail"; retry.channels.clear();
            for (bool summary : {true, false}) {
                const auto rows = TestSequenceResultRows::rows(group, summary);
                QCOMPARE(rows.size(), 8); QVERIFY(rows[0][TestSequenceResultRows::Load] != rows[4][TestSequenceResultRows::Load]);
                QVERIFY(TestSequenceResultRows::rows(retry, summary)[0][TestSequenceResultRows::Max] == "NA");
            }
            QTemporaryDir dir; QString path, error;
            const QString filename = task == "Turn off" ? "turnoff-reference" : "onshort-reference";
            QVERIFY2(TestSequenceExcelReport::save(dir.path(), filename, {r, group, retry}, path, error), qPrintable(error));
            QFile file(path); QVERIFY(file.open(QIODevice::ReadOnly)); const auto bytes = file.readAll();
            QList<QPair<QByteArray,QByteArray>> parts; QVERIFY(ZipArchive::readStored(bytes, parts));
            int sheets = 0;
            for (const auto &part : parts)
                if (part.first.startsWith("xl/worksheets/sheet")) {
                    ++sheets; QVERIFY(part.second.contains("Steady reference (no transient trigger)"));
                    QVERIFY(part.second.contains("20.5")); QVERIFY(part.second.contains("0.91"));
                }
            QCOMPARE(sheets, 2);
            const auto evidence = qEnvironmentVariable("TRIAL_REVIEW_DIR");
            if (!evidence.isEmpty()) {
                QFile output(QDir(evidence).filePath(filename + ".xlsx"));
                QVERIFY(output.open(QIODevice::WriteOnly)); output.write(bytes);
            }
        }
    }
    void transientReferenceFailures_data()
    {
        QTest::addColumn<QString>("task");
        QTest::addColumn<QString>("fault");
        for (const QString &task : {"Turn off", "Turn on then short"})
            for (const QString &fault : {"RMS-error", "MEAN-error", "RMS-na", "MEAN-na",
                                          "stop-reference", "stop-search", "timeout", "cleanup",
                                          "second-error", "secondary-error"})
                QTest::newRow(qPrintable(task + "-" + fault)) << task << fault;
    }
    void transientReferenceFailures()
    {
        QFETCH(QString, task); QFETCH(QString, fault);
        ReferenceScope scope; scope.never = true; scope.fault = fault;
        QAtomicInt stop(0); scope.stop = &stop; auto ctx = context(scope); ctx.target = "MAX";
        if (fault == "second-error") ctx.target = "BOTH";
        if (fault == "timeout") ctx.phaseTimeoutMs = 20;
        if (fault == "cleanup") {
            auto load = ctx.load;
            ctx.load = [load](bool on, QString &error) {
                load(on, error); if (!on) error = "cleanup rejected"; return on;
            };
        }
        ScopeMeasurementStrategy strategy(
            std::unique_ptr<IOscilloscopeMeasureStrategy>(OscilloscopeStrategyFactory::create(task, &ctx)), scope);
        const auto result = strategy.execute(stop);
        const bool legalNa = fault.endsWith("-na");
        QCOMPARE(result.success, legalNa); QVERIFY(result.trials.isEmpty());
        QCOMPARE(result.steadyReferences.size(), legalNa || fault == "cleanup" || fault == "second-error" ? 1 : 0);
        QVERIFY(scope.trace.contains("ACQuire:STATE STOP")); QVERIFY(scope.trace.contains("POWER OFF"));
        QVERIFY(scope.trace.contains("RELAY 0 0")); QVERIFY(scope.trace.contains("LOAD OFF"));
        if (fault.endsWith("-error")) QVERIFY(result.error.contains("reference query failed"));
        if (fault == "cleanup") QVERIFY(result.cleanupFailed);
        if (fault == "second-error")
            QCOMPARE(result.steadyReferences[0].direction, MeasurementDirection::Rise);
        if (legalNa) {
            const auto channels = representativeChannels(result.trials, result.steadyReferences);
            QCOMPARE(channels[0].maximum, 1.); QCOMPARE(channels[0].minimum, .8);
            if (fault == "RMS-na") { QVERIFY(std::isnan(channels[0].rms)); QCOMPARE(channels[0].mean, .9); }
            else { QCOMPARE(channels[0].rms, .91); QVERIFY(std::isnan(channels[0].mean)); }
        }
    }
    void viewAndRetries()
    {
        TestSequenceResultsModel model;
        QWidget window; auto *layout=new QVBoxLayout(&window);
        TestSequenceRunPanel panel; TestSequenceResultDetails details(&model);
        layout->addWidget(&panel);layout->addWidget(&details);
        connect(&panel,&TestSequenceRunPanel::taskSelected,&model,&TestSequenceResultsModel::selectTask);
        panel.setActiveTasks({{42,"Turn on"},{43,"Turn off"}});
        const auto r=record();model.updateRecord(r);
        panel.setTaskResult(0,TaskResultFormatter::measurement(r,true));
        window.resize(1100,650);window.show();
        auto *tasks=panel.findChild<QTableWidget *>("taskRunTable"); tasks->setCurrentCell(0,0);
        QCOMPARE(model.rowCount(),4);
        QVERIFY(!tasks->item(0,4)->text().contains("Max"));
        QCOMPARE(tasks->rowHeight(0),48);
        auto retry=record(2);retry.status="Fail";retry.error="injected failure";model.updateRecord(retry);
        QCOMPARE(model.rowCount(),8);model.updateRecord(retry);QCOMPARE(model.rowCount(),8);
        details.findChild<QComboBox *>("resultDetailMode")->setCurrentIndex(1);QCOMPARE(model.rowCount(),20);
        auto *attempt=details.findChild<QComboBox *>("resultFilter11");attempt->setCurrentIndex(attempt->findData("2"));
        QCOMPARE(model.rowCount(),10);
        auto *direction=details.findChild<QComboBox *>("resultFilter13");direction->setCurrentIndex(direction->findData("Fall"));
        QCOMPARE(model.rowCount(),4);
        auto *channel=details.findChild<QComboBox *>("resultFilter3");channel->setCurrentIndex(channel->findData("CH1"));
        QCOMPARE(model.rowCount(),2);QCOMPARE(model.data(model.index(0,TestSequenceResultRows::Status)).toString(),"Fail");
        tasks->setCurrentCell(1,0);QCOMPARE(model.rowCount(),0);QCOMPARE(attempt->currentIndex(),0);
        tasks->setCurrentCell(0,0);QCOMPARE(model.rowCount(),20);
        const auto path=qEnvironmentVariable("TRIAL_REVIEW_DIR");
        if(!path.isEmpty()) window.grab().save(QDir(path).filePath("result-details.png"));
        model.beginRun();QCOMPARE(model.rowCount(),0);
    }
    void groupAndReport()
    {
        auto r=record();r.taskName="Turn on Group Test";
        r.files={"fixture-only.wfm"};
        r.conditions={{{0,"Same",LoadConditionKind::Static},"Pass",{},r.channels,r.trials},
                      {{1,"Same",LoadConditionKind::Static},"Cancelled","stopped",r.channels,r.trials},
                      {{2,"Skipped",LoadConditionKind::Static},"Skipped",{},{},{}}};
        r.channels.clear();r.trials.clear();r.status="Cancelled";
        auto rows=TestSequenceResultRows::rows(r,true);QCOMPARE(rows.size(),10);
        QVERIFY(rows[0][TestSequenceResultRows::Note].toString().contains("fixture-only.wfm"));
        QVERIFY(!rows[1][TestSequenceResultRows::Note].toString().contains("fixture-only.wfm"));
        QVERIFY(rows[0][TestSequenceResultRows::Load]!=rows[4][TestSequenceResultRows::Load]);
        QCOMPARE(rows[4][TestSequenceResultRows::Status].toString(),"Cancelled");
        const auto text=TaskResultFormatter::measurement(r,false);
        QVERIFY(text.contains("10 trials") && text.contains("3 loads") && text.contains("Incomplete"));
        QTemporaryDir dir;QString path,error;auto hidden=r;hidden.report=false;hidden.externalName="DO_NOT_EXPORT";
        QVERIFY2(TestSequenceExcelReport::save(dir.path(),"results",{r,hidden},path,error),qPrintable(error));
        QFile f(path);QVERIFY(f.open(QIODevice::ReadOnly));auto before=f.readAll();f.close();
        QVERIFY(!before.contains("DO_NOT_EXPORT"));QVERIFY(before.contains("Trials"));
        QVERIFY(TestSequenceExcelReport::save(dir.path(),"results",{r},path,error));
        QVERIFY(f.open(QIODevice::ReadOnly));auto appended=f.readAll();f.close();
        QVERIFY(appended.size()>before.size());
        QLockFile lock(path+".lock");QVERIFY(lock.tryLock());
        QVERIFY(!TestSequenceExcelReport::save(dir.path(),"results",{r},path,error));lock.unlock();
        QList<QPair<QByteArray,QByteArray>> parts;QVERIFY(ZipArchive::readStored(appended,parts));
        for(auto &part:parts) if(part.first=="xl/worksheets/sheet2.xml") part.second.replace("Trigger Level","Edited header");
        const auto edited=ZipArchive::store(parts);QVERIFY(f.open(QIODevice::WriteOnly));f.write(edited);f.close();
        QVERIFY(!TestSequenceExcelReport::save(dir.path(),"results",{r},path,error));
        QVERIFY(f.open(QIODevice::ReadOnly));QCOMPARE(f.readAll(),edited);f.close();
        const auto evidence=qEnvironmentVariable("TRIAL_REVIEW_DIR");
        if(!evidence.isEmpty()) {QFile output(QDir(evidence).filePath("trial-results.xlsx"));QVERIFY(output.open(QIODevice::WriteOnly));output.write(appended);}
    }
};
QTEST_MAIN(TrialResultsTest)
#include "trial_results_test.moc"
