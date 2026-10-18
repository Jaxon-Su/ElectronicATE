#include "testsequenceexcelreport.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QLockFile>
#include <iostream>
#include <stdexcept>
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main(int argc, char** argv) {
 QCoreApplication app(argc, argv);
 try {
  QTemporaryDir temporary;
  const QString directory = argc > 1 ? QString::fromLocal8Bit(argv[1]) : temporary.path();
  QString error, path;
  for (const QString& name : {"", "../bad", "CON", "bad?", "file.", "a/b"})
   require(!TestSequenceExcelReport::validate(directory,name,error), "invalid name accepted");
  require(!TestSequenceExcelReport::validate("relative", "test", error),"relative folder accepted");
  QVector<TestSequenceResultRecord> records;
  TestSequenceResultRecord r; r.taskIndex=0;r.attempt=1;r.taskName="Static Test";r.externalName="=1+1 測試 & < >";r.status="Pass";
  r.channels.append({1,"A",1e-8,-2e-9,3.0,std::numeric_limits<double>::quiet_NaN()});records<<r;
  r.taskIndex=1;r.taskName="Delay";r.channels.clear();r.summary="Waited 5 s";records<<r;
  r.taskIndex=2;r.taskName="Write Oscilloscope";r.summary="";r.error="Transport disconnected";r.status="Fail";r.attempt=2;records<<r;
  r.taskIndex=3;r.report=false;r.taskName="Hidden";records<<r;
  r.taskIndex=4;r.report=true;r.taskName="Turn off";r.status="Skipped";r.attempt=0;r.error="Stopped before execution";records<<r;
  require(TestSequenceExcelReport::save(directory,"測試.xlsx",records,path,error),qPrintable(error));
  QFile file(path);require(file.open(QIODevice::ReadOnly),"report missing");
  auto bytes=file.readAll();require(bytes.startsWith("PK"),"not XLSX ZIP");
  require(!bytes.contains("Hidden"),"unchecked Report exported");
  file.close();
  QString second;require(TestSequenceExcelReport::save(directory,"測試.xlsx",records,second,error),"second save failed");
  require(path==second && QFile::exists(path),"report path changed");
  require(file.open(QIODevice::ReadOnly),"report missing after append");
  const auto appended=file.readAll();file.close();
  require(appended.count("Waited 5 s")==4 && appended.count(">Seq<")==2,"rows not appended on both sheets or header duplicated");
  require(TestSequenceExcelReport::save(directory,"new report",records,second,error),"renamed report failed");
  require(second!=path,"renamed report reused old path");
  QLockFile lock(path+".lock");require(lock.tryLock(),"test lock failed");
  require(!TestSequenceExcelReport::save(directory,"測試",records,second,error),"concurrent append allowed");lock.unlock();
  QFile bad(QDir(directory).filePath("corrupt.xlsx"));require(bad.open(QIODevice::WriteOnly),"bad setup");bad.write("keep existing data");bad.close();
  require(!TestSequenceExcelReport::save(directory,"corrupt",records,second,error),"corrupt report replaced");
  require(bad.open(QIODevice::ReadOnly) && bad.readAll()=="keep existing data","existing data lost");
  require(file.open(QIODevice::ReadOnly) && file.readAll()==appended,"existing report changed on failure");file.close();
  require(!TestSequenceExcelReport::save(directory+"/missing","test",records,second,error),"bad folder saved");
  std::cout<<path.toUtf8().constData()<<"\n";
 } catch(const std::exception& ex) { std::cerr<<ex.what();return 1; }
}
