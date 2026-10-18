#include "resourcecleaner.h"
#include "icommunication.h"
#include "instrumentwithcommbase.h"
#include <stdexcept>
class Comm : public ICommunication {
public:
    Comm(int& destroyed,bool fail):destroyed(destroyed),fail(fail){}
    ~Comm() override {++destroyed;}
    int& destroyed;bool fail;
    bool open() override{return true;}
    void close() override{if(fail)throw std::runtime_error("close failed");}
    int write(const QByteArray&) override{return 0;}
    int read(QByteArray&,int) override{return 0;}
    bool isOpen() const override{return true;}
    QString lastError() const override{return {};}
};
class Driver:public InstrumentWithCommBase {
public:using InstrumentWithCommBase::InstrumentWithCommBase;
QString model()const override{return "fake";} QString vendor()const override{return "fake";}
};
int main(){
    int destroyed=0;
    { Comm comm(destroyed,true); { Driver driver(&comm); driver.connect(); driver.disconnect();
      if(driver.isConnected()||!driver.lastError().contains("close failed"))return 3;
      driver.connect(); } }
    if(destroyed!=1)return 2;
    destroyed=0;QVector<DCLoad*> loads;
    QMap<QString,ICommunication*> comms{{"first",new Comm(destroyed,true)},{"second",new Comm(destroyed,false)}};
    ResourceCleaner::cleanupDCLoads(loads,comms);
    return destroyed==2&&comms.isEmpty()?0:1;
}
