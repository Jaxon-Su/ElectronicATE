#pragma once

#include <QObject>
#include <QVector>
#include <QMap>
#include <QString>
#include "page1config.h"
#include "page2config.h"


class ACSource;
class DCLoad;
class RelayBase;
class ICommunication;


class InstrumentCreator {
public:

    struct ACSourceResult {
        ACSource* source = nullptr;
        ICommunication* comm = nullptr;
        bool success = false;
    };


    struct DCLoadResult {
        QVector<DCLoad*> dcLoads;
        QMap<QString, ICommunication*> commMap;
        bool success = false;
    };


    struct RelayResult {
        QVector<RelayBase*> relays;
        QMap<QString, ICommunication*> commMap;
        bool success = false;
        QString errorMessage;
    };

    static ACSourceResult createACSource(
        const Page1Config& config,
        QObject* viewModel
        );


    static DCLoadResult createDCLoads(
        const Page1Config& config,
        QObject* viewModel,
        TableKind kind
        );


    static RelayResult createRelays(
        const Page1Config& config,
        QObject* viewModel
        );

private:

    InstrumentCreator() = delete;
    ~InstrumentCreator() = delete;
    InstrumentCreator(const InstrumentCreator&) = delete;
    InstrumentCreator& operator=(const InstrumentCreator&) = delete;
};
