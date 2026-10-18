#include "instrumentcreator.h"
#include "communicationfactory.h"
#include "relayfactory.h"
#include "relay.h"
#include "dcloadfactory.h"
#include "acsourcefactory.h"
#include <stdexcept>
#include <iostream>
#include <type_traits>
static int created=0,destroyed=0;
static bool throwCreate=false, allowDrivers=false;
static int driverCreated=0,driverDestroyed=0;
class Load : public DCLoad {
public:
 explicit Load(ICommunication *c):DCLoad(c){++driverCreated;}
 ~Load()override{++driverDestroyed;}
 QString model()const override{return "fake";}
 QString vendor()const override{return "fake";}
 void setLoadOn()override{}
 void setLoadOff()override{}
 void setStaticCurrent(const StaticCurrentParam&)override{}
 void setDynamicCurrent(const DynamicCurrentParam&)override{}
};
class Relay : public RelayBase {
public:
 explicit Relay(ICommunication *c):RelayBase(c){++driverCreated;}
 ~Relay()override{++driverDestroyed;}
 QString model()const override{return "fake";}
 QString vendor()const override{return "fake";}
 bool turnOn(int)override{return true;}
 bool turnOff(int)override{return true;}
 bool setMultiChannels(const QVector<int>&,bool)override{return true;}
 RelayStatus getStatus(int)override{return RelayStatus::Open;}
};
class Comm:public ICommunication {
public:
 ~Comm()override{++destroyed;}
 bool open()override{return true;}
 void close()override{throw std::runtime_error("close failed");}
 bool isOpen()const override{return true;}
 int read(QByteArray&,int)override{return -1;}
 int write(const QByteArray&)override{return -1;}
 QString lastError()const override{return {};}
};
ICommunication* CommunicationFactory::create(const QString&){if(throwCreate&&created==1)throw 42;++created;return new Comm;}
RelayBase* RelayFactory::createRelay(const QString&,ICommunication* c,quint8){return allowDrivers?new Relay(c):nullptr;}
DCLoad* DCLoadFactory::createDCLoad(const QString&,ICommunication* c){return allowDrivers?new Load(c):nullptr;}
ACSource* ACSourceFactory::createACSource(const QString&,ICommunication*){return nullptr;}
int main(){try{
 for(bool unknown:{false,true})for(const QString type:{"Relay","Load"}){
    created=destroyed=0;throwCreate=unknown;InstrumentSetupConfig config;
    for(int i=0;i<2;++i){InstrumentConfig item;item.type=type;item.name=type+QString::number(i);item.modelName="test";item.commConfig.protocol=ProtocolType::GPIB;item.commConfig.gpibAddress=i+1;config.instruments<<item;}
    if(type=="Relay"){auto result=InstrumentCreator::createRelays(config);if(result.success||!result.commMap().isEmpty())return 1;}
    else {auto result=InstrumentCreator::createDCLoads(config,TableKind::Load);if(result.success||!result.commMap().isEmpty())return 2;}
    if(created!=destroyed)return 3;
 }
 static_assert(!std::is_copy_constructible_v<InstrumentCreator::ACSourceResult>);
 static_assert(!std::is_copy_constructible_v<InstrumentCreator::DCLoadResult>);
 static_assert(!std::is_copy_constructible_v<InstrumentCreator::RelayResult>);
 static_assert(std::is_same_v<decltype(std::declval<InstrumentCreator::DCLoadResult&>().dcLoads()), const QVector<DCLoad*>&>);
 static_assert(std::is_same_v<decltype(std::declval<InstrumentCreator::RelayResult&>().relays()), const QVector<RelayBase*>&>);
 created=destroyed=0;throwCreate=false;
 {
    InstrumentCreator::ACSourceResult first(nullptr,CommunicationFactory::create("first"),true);
    auto second=std::move(first);
    if(first.comm() || !second.comm())return 5;
    InstrumentCreator::ACSourceResult third(nullptr,CommunicationFactory::create("second"),false);
    third=std::move(second);
    if(destroyed!=1 || second.comm())return 6;
 }
 if(created!=destroyed)return 7;
 {
    allowDrivers=true;
    InstrumentSetupConfig config;InstrumentConfig load;load.type="Load";load.name="Load1";load.modelName="test";load.commConfig.protocol=ProtocolType::GPIB;
    load.channels.resize(1);load.channels[0].subModel="fake";load.channels[0].index=1;load.channelNumbers={1};config.instruments={load};
    auto first=InstrumentCreator::createDCLoads(config,TableKind::Load);
    if(!first.success || first.dcLoads().size()!=1)return 11;
    auto second=std::move(first);
    if(!first.commMap().isEmpty())return 8;
    InstrumentConfig relayConfig;relayConfig.type="Relay";relayConfig.name="Relay1";relayConfig.modelName="fake";relayConfig.commConfig.protocol=ProtocolType::GPIB;relayConfig.commConfig.gpibAddress=2;
    config.instruments={relayConfig};auto relay=InstrumentCreator::createRelays(config);
    if(!relay.success || relay.relays().size()!=1)return 12;
    auto next=std::move(relay);
    if(!relay.commMap().isEmpty())return 9;
 }
 if(created!=destroyed || driverCreated!=driverDestroyed)return 10;
 return 0;
}catch(...){return 4;}}
