#include "reportwriter.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <atomic>
#include <memory>
#include <stdexcept>
#include <iostream>
void require(bool ok,const char* error){if(!ok)throw std::runtime_error(error);}
int main(int argc,char**argv){QCoreApplication app(argc,argv);try{
    for(bool fail:{false,true}){
        std::atomic<int> writes{0};
        ReportWriter writer([&](const QString& directory,const QString& name,const auto& records){
            ++writes;QThread::msleep(30);
            if(fail)throw std::runtime_error("disk failure");
            require(directory=="dir"&&name=="name"&&records.size()==1,"snapshot lost");
            return ReportWriteResult{true,"file.xlsx",{}};
        },nullptr);
        QEventLoop loop;bool delivered=false;
        QObject::connect(&writer,&ReportWriter::completed,[&](bool ok,const auto&,const auto& error){
            require(!writer.isBusy(),"completion still busy");
            require(ok!=fail && (!fail||error=="disk failure"),"error result lost");delivered=true;loop.quit();
        });
        require(writer.submit("dir","name",{TestSequenceResultRecord{}}),"start failed");
        require(!writer.submit("dir","name",{}),"parallel write allowed");
        QTimer::singleShot(2000,&loop,&QEventLoop::quit);loop.exec();
        require(delivered&&writes==1,"completion missing");
    }
    auto calls=std::make_shared<std::atomic<int>>(0);
    auto writer=std::make_unique<ReportWriter>([calls](const auto&,const auto&,const auto&){
        QThread::msleep(30);++*calls;return ReportWriteResult{true,{},{}};
    },nullptr);
    writer->submit("dir","name",{});writer.reset();
    QEventLoop loop;QTimer::singleShot(150,&loop,&QEventLoop::quit);loop.exec();
    require(*calls==1,"destroyed owner cancelled owned snapshot");return 0;
}catch(const std::exception&e){std::cerr<<e.what();return 1;}}
