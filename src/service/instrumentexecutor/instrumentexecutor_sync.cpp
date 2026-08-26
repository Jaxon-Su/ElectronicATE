#include "instrumentexecutor.h"

#include "dcload.h"
#include "resourcecleaner.h"

#include <QDebug>
#include <QThread>
#include <utility>

namespace InstrumentExecutorInternal {

DCLoadSession::DCLoadSession(InstrumentCreator::DCLoadResult& result)
    : m_result(result)
{
}

DCLoadSession::~DCLoadSession()
{
    ResourceCleaner::cleanupDCLoads(m_result.dcLoads, m_result.commMap);
}

MasterSlaveSyncPlan MasterSlaveSyncPlanner::classify(const QList<DCLoad*>& dcLoads)
{
    MasterSlaveSyncPlan plan;

    QString errorMessage;
    if (!capture(dcLoads, plan.snapshots, errorMessage)) {
        plan.valid = false;
        plan.errorMessage = errorMessage;
        return plan;
    }

    const int masterCount = countType(plan.snapshots, 1);
    const int slaveCount = countType(plan.snapshots, 2);

    if (masterCount > 1) {
        plan.valid = false;
        plan.errorMessage =
            "Sync configuration error: multiple masters were found. "
            "Please set exactly one load as MASTER, or set all loads to NONE.";
        return plan;
    }

    if (masterCount == 0 && slaveCount > 0) {
        plan.valid = false;
        plan.errorMessage =
            "Sync configuration error: slave load exists but no master was found. "
            "Please set one load as MASTER, or set all loads to NONE.";
        return plan;
    }

    plan.active = (masterCount == 1);
    plan.master = master(plan.snapshots);
    return plan;
}

void MasterSlaveSyncPlanner::setAll(const QList<SyncTypeSnapshot>& snapshots, int type)
{
    qDebug() << "[SyncPlanner] setAll" << syncTypeName(type)
             << "snapshotCount=" << snapshots.size();
    apply(snapshots, type);
}

void MasterSlaveSyncPlanner::restore(const QList<SyncTypeSnapshot>& snapshots)
{
    qDebug() << "[SyncPlanner] restore snapshotCount=" << snapshots.size();
    apply(snapshots, -1);
}

void MasterSlaveSyncPlanner::sendLoadToIndependentMembers(
    const QList<DCLoad*>& dcLoads,
    const QList<SyncTypeSnapshot>& snapshots,
    bool on)
{
    for (DCLoad* dcLoad : dcLoads) {
        const int type = typeFor(dcLoad, snapshots);
        if (type == 1 || type == 2)
            continue;

        dcLoad->setChannel(dcLoad->realChannel());
        if (on)
            dcLoad->setLoadOn();
        else
            dcLoad->setLoadOff();
    }
}

QList<DCLoad*> MasterSlaveSyncPlanner::devices(const QList<DCLoad*>& dcLoads)
{
    QList<DCLoad*> result;
    QStringList seenKeys;

    for (DCLoad* dcLoad : dcLoads) {
        if (dcLoad->syncCapability() != LoadSyncCapability::MasterSlaveSyncType)
            continue;

        const QString key = dcLoad->syncIdentityKey();
        if (key.isEmpty() || seenKeys.contains(key))
            continue;

        seenKeys.append(key);
        result.append(dcLoad);
    }

    return result;
}

bool MasterSlaveSyncPlanner::capture(
    const QList<DCLoad*>& dcLoads,
    QList<SyncTypeSnapshot>& snapshots,
    QString& errorMessage)
{
    snapshots.clear();

    const QList<DCLoad*> syncDevices = devices(dcLoads);
    QList<DCLoad*> deferredDevices;
    int queryableMasterCount = 0;

    for (DCLoad* dcLoad : syncDevices) {
        dcLoad->setChannel(dcLoad->realChannel());
        if (!dcLoad->canQuerySyncType()) {
            deferredDevices.append(dcLoad);
            continue;
        }

        const int type = dcLoad->syncType();
        qDebug() << "[SyncPlanner] capture"
                 << "model=" << dcLoad->model()
                 << "address=" << dcLoad->getaddress()
                 << "realChannel=" << dcLoad->realChannel()
                 << "identityKey=" << dcLoad->syncIdentityKey()
                 << "canQuery=" << dcLoad->canQuerySyncType()
                 << "type=" << syncTypeName(type);
        if (type < 0 || type > 2) {
            errorMessage = QString("Failed to read sync type: %1 CHAN %2")
                               .arg(dcLoad->model())
                               .arg(dcLoad->realChannel());
            if (!dcLoad->lastError().isEmpty())
                errorMessage += QString(" (%1)").arg(dcLoad->lastError());
            return false;
        }

        snapshots.append({ dcLoad, type });
        if (type == 1)
            ++queryableMasterCount;
    }

    for (DCLoad* dcLoad : std::as_const(deferredDevices)) {
        dcLoad->setChannel(dcLoad->realChannel());
        int type = dcLoad->defaultSyncType();
        if (type < 0 && queryableMasterCount > 0)
            type = 2;
        qDebug() << "[SyncPlanner] capture deferred"
                 << "model=" << dcLoad->model()
                 << "address=" << dcLoad->getaddress()
                 << "realChannel=" << dcLoad->realChannel()
                 << "identityKey=" << dcLoad->syncIdentityKey()
                 << "queryableMasterCount=" << queryableMasterCount
                 << "type=" << syncTypeName(type);
        if (type < 0 || type > 2) {
            errorMessage = QString("Failed to determine sync type: %1 CHAN %2")
                               .arg(dcLoad->model())
                               .arg(dcLoad->realChannel());
            return false;
        }

        snapshots.append({ dcLoad, type });
    }

    return true;
}

void MasterSlaveSyncPlanner::apply(const QList<SyncTypeSnapshot>& snapshots, int overrideType)
{
    const auto applyMatchingType = [&](int expectedType) {
        for (const SyncTypeSnapshot& snapshot : snapshots) {
            if (!snapshot.load)
                continue;

            const int type = (overrideType >= 0) ? overrideType : snapshot.type;
            if (type != expectedType)
                continue;

            if (!snapshot.load->shouldApplySyncTypeTransition(type)) {
                qDebug() << "[SyncPlanner] skip apply"
                         << "targetType=" << syncTypeName(type)
                         << "model=" << snapshot.load->model()
                         << "address=" << snapshot.load->getaddress()
                         << "realChannel=" << snapshot.load->realChannel()
                         << "identityKey=" << snapshot.load->syncIdentityKey();
                continue;
            }

            qDebug() << "[SyncPlanner] apply"
                     << "overrideType=" << syncTypeName(overrideType)
                     << "targetType=" << syncTypeName(type)
                     << "model=" << snapshot.load->model()
                     << "address=" << snapshot.load->getaddress()
                     << "realChannel=" << snapshot.load->realChannel()
                     << "identityKey=" << snapshot.load->syncIdentityKey();
            snapshot.load->setChannel(snapshot.load->realChannel());
            snapshot.load->setSyncType(type);
        }
    };

    // Keep wired sync buses quiet while roles change:
    // neutral members first, slaves next, master last.
    applyMatchingType(0);
    applyMatchingType(2);
    applyMatchingType(1);
}

QString MasterSlaveSyncPlanner::syncTypeName(int type)
{
    switch (type) {
    case -1: return QStringLiteral("RESTORE");
    case 0: return QStringLiteral("NONE");
    case 1: return QStringLiteral("MASTER");
    case 2: return QStringLiteral("SLAVE");
    default: return QStringLiteral("INVALID(%1)").arg(type);
    }
}

int MasterSlaveSyncPlanner::countType(const QList<SyncTypeSnapshot>& snapshots, int type)
{
    int count = 0;
    for (const SyncTypeSnapshot& snapshot : snapshots) {
        if (snapshot.type == type)
            ++count;
    }
    return count;
}

DCLoad* MasterSlaveSyncPlanner::master(const QList<SyncTypeSnapshot>& snapshots)
{
    for (const SyncTypeSnapshot& snapshot : snapshots) {
        if (snapshot.type == 1)
            return snapshot.load;
    }
    return nullptr;
}

int MasterSlaveSyncPlanner::typeFor(DCLoad* dcLoad, const QList<SyncTypeSnapshot>& snapshots)
{
    if (!dcLoad || dcLoad->syncCapability() != LoadSyncCapability::MasterSlaveSyncType)
        return -1;

    const QString key = dcLoad->syncIdentityKey();
    for (const SyncTypeSnapshot& snapshot : snapshots) {
        if (snapshot.load && snapshot.load->syncIdentityKey() == key)
            return snapshot.type;
    }

    return -1;
}

namespace {

void sleepMs(int milliseconds)
{
    QThread::msleep(milliseconds);
}

void stopSyncRun(DCLoad* master)
{
    if (!master)
        return;

    master->setChannel(master->realChannel());
    master->setSyncRun(false);
    sleepMs(kSyncRunSettleMs);
}

void prepareSyncMembersForProgramming(const MasterSlaveSyncPlan& syncPlan)
{
    if (!syncPlan.active)
        return;

    stopSyncRun(syncPlan.master);
    MasterSlaveSyncPlanner::setAll(syncPlan.snapshots, 0);
    sleepMs(kSyncTypeSettleMs);
}

void restoreSyncMembersAndStart(const MasterSlaveSyncPlan& syncPlan, int runSettleMs)
{
    if (!syncPlan.active || !syncPlan.master)
        return;

    MasterSlaveSyncPlanner::restore(syncPlan.snapshots);
    sleepMs(kSyncTypeSettleMs);
    syncPlan.master->setChannel(syncPlan.master->realChannel());
    syncPlan.master->setSyncRun(true);
    sleepMs(runSettleMs);
}

void stopSyncAndLoadOff(DCLoad* master, int postLoadOffSettleMs)
{
    if (!master)
        return;

    stopSyncRun(master);
    master->setLoadOff();
    sleepMs(postLoadOffSettleMs);
}

void executeIndependentStaticLoadAction(const QVector<DCLoad*>& dcLoads, LoadAction action)
{
    for (DCLoad* dcLoad : dcLoads) {
        const int realindex = dcLoad->realChannel();
        dcLoad->setChannel(realindex);

        if (action == LoadAction::LoadOff) {
            qDebug() << "[runLoad]   LOAD OFF  model=" << dcLoad->model() << "CHAN=" << realindex;
            dcLoad->setLoadOff();
        } else if (action == LoadAction::LoadOn) {
            qDebug() << "[runLoad]   LOAD ON   model=" << dcLoad->model() << "CHAN=" << realindex;
            dcLoad->setLoadOn();
        }
    }
}

void executeSynchronizedStaticLoadAction(
    const QVector<DCLoad*>& dcLoads,
    const MasterSlaveSyncPlan& syncPlan,
    LoadAction action)
{
    DCLoad* master = syncPlan.master;
    const int realindex = master->realChannel();
    master->setChannel(realindex);

    if (action == LoadAction::LoadOff) {
        qDebug() << "[runLoad]   SYNC LOAD OFF master only model=" << master->model() << "CHAN=" << realindex;
        stopSyncAndLoadOff(master, kLoadOffSettleMs);
        MasterSlaveSyncPlanner::sendLoadToIndependentMembers(dcLoads, syncPlan.snapshots, false);
        return;
    }

    restoreSyncMembersAndStart(syncPlan, kSyncRunSettleMs);
    qDebug() << "[runLoad]   SYNC LOAD ON master only model=" << master->model() << "CHAN=" << realindex;
    master->setLoadOn();
    MasterSlaveSyncPlanner::sendLoadToIndependentMembers(dcLoads, syncPlan.snapshots, true);
}

void executeIndependentDynamicLoadAction(const QVector<DCLoad*>& dcLoads, DyLoadAction action)
{
    for (DCLoad* dcLoad : dcLoads) {
        const int realindex = dcLoad->realChannel();
        dcLoad->setChannel(realindex);

        if (action == DyLoadAction::DyloadOff)
            dcLoad->setLoadOff();
        else if (action == DyLoadAction::DyLoadOn)
            dcLoad->setLoadOn();
    }
}

void executeSynchronizedDynamicLoadAction(
    const QVector<DCLoad*>& dcLoads,
    const MasterSlaveSyncPlan& syncPlan,
    DyLoadAction action)
{
    DCLoad* master = syncPlan.master;

    if (action == DyLoadAction::DyloadOff) {
        stopSyncAndLoadOff(master, kDynamicRunSettleMs);
        MasterSlaveSyncPlanner::sendLoadToIndependentMembers(dcLoads, syncPlan.snapshots, false);
        return;
    }

    restoreSyncMembersAndStart(syncPlan, kDynamicRunSettleMs);
    master->setLoadOn();
    MasterSlaveSyncPlanner::sendLoadToIndependentMembers(dcLoads, syncPlan.snapshots, true);
}

QString configuredSyncTypeName(int type)
{
    switch (type) {
    case 0: return QStringLiteral("NONE");
    case 1: return QStringLiteral("MASTER");
    case 2: return QStringLiteral("SLAVE");
    default: return QStringLiteral("INVALID(%1)").arg(type);
    }
}

bool applyConfiguredSyncTypes(const QVector<DCLoad*>& dcLoads, QString& errorMessage)
{
    QStringList seenKeys;
    QList<DCLoad*> configuredDevices;

    for (DCLoad* dcLoad : dcLoads) {
        if (!dcLoad || dcLoad->syncCapability() != LoadSyncCapability::MasterSlaveSyncType)
            continue;

        const QString key = dcLoad->syncIdentityKey();
        if (key.isEmpty() || seenKeys.contains(key))
            continue;
        seenKeys.append(key);

        const int type = dcLoad->configuredSyncType();
        if (type < 0)
            continue;
        if (type > 2) {
            errorMessage = QString("Invalid configured sync type %1: %2 CHAN %3")
                               .arg(type)
                               .arg(dcLoad->model())
                               .arg(dcLoad->realChannel());
            return false;
        }

        configuredDevices.append(dcLoad);
    }

    if (configuredDevices.isEmpty())
        return true;

    bool stoppedRun = false;
    for (DCLoad* dcLoad : std::as_const(configuredDevices)) {
        if (dcLoad->configuredSyncType() != 1)
            continue;

        qDebug() << "[SyncPlanner] configured master SYNC:RUN OFF before role apply"
                 << "model=" << dcLoad->model()
                 << "address=" << dcLoad->getaddress()
                 << "realChannel=" << dcLoad->realChannel()
                 << "identityKey=" << dcLoad->syncIdentityKey();
        dcLoad->setChannel(dcLoad->realChannel());
        dcLoad->setSyncRun(false);
        stoppedRun = true;
    }
    if (stoppedRun)
        sleepMs(kSyncRunSettleMs);

    const auto applyType = [&](int type, bool useConfiguredType) {
        bool applied = false;

        for (DCLoad* dcLoad : std::as_const(configuredDevices)) {
            const int targetType = useConfiguredType ? dcLoad->configuredSyncType() : type;
            if (targetType != type)
                continue;

            if (!dcLoad->shouldApplySyncTypeTransition(targetType)) {
                qDebug() << "[SyncPlanner] skip configured sync type apply"
                         << "type=" << configuredSyncTypeName(targetType)
                         << "model=" << dcLoad->model()
                         << "address=" << dcLoad->getaddress()
                         << "realChannel=" << dcLoad->realChannel()
                         << "identityKey=" << dcLoad->syncIdentityKey();
                continue;
            }

            qDebug() << "[SyncPlanner] apply configured sync type"
                     << "type=" << configuredSyncTypeName(targetType)
                     << "model=" << dcLoad->model()
                     << "address=" << dcLoad->getaddress()
                     << "realChannel=" << dcLoad->realChannel()
                     << "identityKey=" << dcLoad->syncIdentityKey();
            dcLoad->setChannel(dcLoad->realChannel());
            dcLoad->setSyncType(targetType);
            applied = true;
        }

        if (applied)
            sleepMs(kSyncTypeSettleMs);
    };

    // With the sync cable connected, avoid transient master-first states:
    // first neutralize every configured member, then restore slaves, then master.
    applyType(0, false);
    applyType(2, true);
    applyType(1, true);

    return true;
}

} // namespace

bool buildSyncPlanIfNeeded(
    const QVector<DCLoad*>& dcLoads,
    bool shouldBuild,
    MasterSlaveSyncPlan& syncPlan,
    QString& errorMessage)
{
    if (!shouldBuild)
        return true;

    if (!applyConfiguredSyncTypes(dcLoads, errorMessage))
        return false;

    syncPlan = MasterSlaveSyncPlanner::classify(dcLoads);
    if (syncPlan.valid)
        return true;

    errorMessage = syncPlan.errorMessage;
    return false;
}

void prepareStaticLoadProgrammingIfNeeded(const MasterSlaveSyncPlan& syncPlan, LoadAction action)
{
    if (syncPlan.active && action != LoadAction::LoadOff)
        prepareSyncMembersForProgramming(syncPlan);
}

void prepareDynamicLoadProgrammingIfNeeded(const MasterSlaveSyncPlan& syncPlan, DyLoadAction action)
{
    if (syncPlan.active && action != DyLoadAction::DyloadOff)
        prepareSyncMembersForProgramming(syncPlan);
}

bool handleStaticLoadConfigurationFailure(
    LoadAction action,
    const QStringList& failedChannels,
    const MasterSlaveSyncPlan& syncPlan,
    InstrumentExecutor::Result& result)
{
    if (failedChannels.isEmpty())
        return false;

    const QString errorMessage =
        "Some load channels failed to configure: " + failedChannels.join("; ");

    if (action == LoadAction::LoadOn) {
        qWarning() << "[runLoad] LOAD ON aborted -" << errorMessage;
        MasterSlaveSyncPlanner::restore(syncPlan.snapshots);
        result = { false, errorMessage + ". LOAD ON aborted." };
        return true;
    }

    if (action == LoadAction::Change) {
        qWarning() << "[runLoad] LOAD CHANGE partially applied -" << errorMessage;
        MasterSlaveSyncPlanner::restore(syncPlan.snapshots);
        result = { false, errorMessage + ". Successful channels were updated." };
        return true;
    }

    return false;
}

bool handleDynamicLoadConfigurationFailure(
    DyLoadAction action,
    const QStringList& failedChannels,
    const MasterSlaveSyncPlan& syncPlan,
    InstrumentExecutor::Result& result)
{
    if (failedChannels.isEmpty())
        return false;

    const QString errorMessage =
        "Some dynamic load channels failed to configure: " + failedChannels.join("; ");

    if (action == DyLoadAction::DyLoadOn) {
        qWarning() << "[runDyLoad] DYLOAD ON aborted -" << errorMessage;
        MasterSlaveSyncPlanner::restore(syncPlan.snapshots);
        result = { false, errorMessage + ". Dynamic LOAD ON aborted." };
        return true;
    }

    if (action == DyLoadAction::Change) {
        qWarning() << "[runDyLoad] DYLOAD CHANGE partially applied -" << errorMessage;
        MasterSlaveSyncPlanner::restore(syncPlan.snapshots);
        result = { false, errorMessage + ". Successful channels were updated." };
        return true;
    }

    return false;
}

void executeStaticLoadOutputStep(
    const QVector<DCLoad*>& dcLoads,
    const MasterSlaveSyncPlan& syncPlan,
    LoadAction action)
{
    qDebug() << "[runLoad] Pass2 - send LOAD ON/OFF:";

    if (syncPlan.active && (action == LoadAction::LoadOn || action == LoadAction::LoadOff))
        executeSynchronizedStaticLoadAction(dcLoads, syncPlan, action);
    else
        executeIndependentStaticLoadAction(dcLoads, action);
}

void executeDynamicLoadOutputStep(
    const QVector<DCLoad*>& dcLoads,
    const MasterSlaveSyncPlan& syncPlan,
    DyLoadAction action)
{
    if (syncPlan.active && (action == DyLoadAction::DyLoadOn || action == DyLoadAction::DyloadOff))
        executeSynchronizedDynamicLoadAction(dcLoads, syncPlan, action);
    else
        executeIndependentDynamicLoadAction(dcLoads, action);
}

} // namespace InstrumentExecutorInternal
