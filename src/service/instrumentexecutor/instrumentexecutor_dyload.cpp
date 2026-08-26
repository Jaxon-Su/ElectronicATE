#include "instrumentexecutor.h"

#include "dcload.h"

#include <QDebug>

namespace {

bool configureDCDyLoad(
    DCLoad* dcLoad,
    int index,
    const DataFinder::DyLoadDataResult& dataInfo,
    const QVector<QString>& vons,
    const QVector<QString>& outputVoltages,
    const QVector<QString>& ranges)
{
    if (!dataInfo.found) {
        qWarning() << "[configureDCDyLoad] dataInfo not found, skip."
                   << "model=" << dcLoad->model()
                   << "CHAN=" << dcLoad->realChannel()
                   << "outputIndex=" << index;
        return false;
    }
    if (index <= 0 || index > dataInfo.values.size()) {
        qWarning() << "[configureDCDyLoad] outputIndex out of range:"
                   << "model=" << dcLoad->model()
                   << "CHAN=" << dcLoad->realChannel()
                   << "outputIndex=" << index
                   << "valuesSize=" << dataInfo.values.size();
        return false;
    }

    QString strValue = dataInfo.values[index - 1].trimmed();
    if (strValue.isEmpty()) {
        qWarning() << "[configureDCDyLoad] value is empty:"
                   << "model=" << dcLoad->model()
                   << "CHAN=" << dcLoad->realChannel()
                   << "outputIndex=" << index;
        return false;
    }

    int realindex = dcLoad->realChannel();
    qDebug() << "[configureDCDyLoad] model=" << dcLoad->model()
             << "CHAN=" << realindex
             << "outputIndex=" << index
             << "value=" << strValue
             << "t1t2=" << dataInfo.t1t2;

    dcLoad->setChannel(realindex);
    InstrumentExecutor::applyDyLoadSettings(
        dcLoad, index, strValue, dataInfo.t1t2, vons, outputVoltages, ranges);
    return true;
}

QString describeLoadChannel(DCLoad* dcLoad)
{
    if (!dcLoad)
        return {};

    return QString("%1 CHAN %2 outputIndex %3")
        .arg(dcLoad->model())
        .arg(dcLoad->realChannel())
        .arg(dcLoad->channelIndex());
}

} // namespace

namespace InstrumentExecutorInternal {

QStringList configureDynamicLoadChannels(
    const QVector<DCLoad*>& dcLoads,
    const DataFinder::DyLoadDataResult& dataInfo,
    const DynamicMetaRow& meta)
{
    QStringList failedChannels;

    for (DCLoad* dcLoad : std::as_const(dcLoads)) {
        const bool configured =
            configureDCDyLoad(dcLoad, dcLoad->channelIndex(),
                              dataInfo, meta.von, meta.vo, meta.ranges);
        if (!configured)
            failedChannels << describeLoadChannel(dcLoad);
    }

    return failedChannels;
}

} // namespace InstrumentExecutorInternal

void InstrumentExecutor::applyDyLoadValueSettings(
    DCLoad* dcLoad, int index, const QString& value,
    const QString& dyTime, const QVector<QString>& outputVoltages,
    const QVector<QString>& ranges)
{
    int nSegments = dcLoad->getNumSegments();
    DynamicCurrentParam param;

    const QStringList currentParts = value.split('~');
    for (const auto& part : currentParts) {
        bool ok = false;
        double v = part.trimmed().toDouble(&ok);
        if (ok && param.levels.size() < nSegments) {
            param.levels.append(v);
            param.enabledMask.append(true);
        }
    }
    if (param.levels.size() == 1 && nSegments >= 2) {
        param.levels.append(param.levels[0]);
        param.enabledMask.append(true);
    }

    const QStringList timeParts = dyTime.split('~');
    for (const auto& part : timeParts) {
        bool ok = false;
        double v = part.trimmed().toDouble(&ok);
        if (ok && param.timings.size() < 2)
            param.timings.append(v / 1000.0);
    }
    if (param.timings.size() == 1)
        param.timings.append(param.timings[0]);
    if (param.timings.isEmpty()) {
        param.timings.append(0.00001);
        param.timings.append(0.00001);
    }

    if (index - 1 < outputVoltages.size()) {
        bool ok;
        double voltage = outputVoltages[index - 1].toDouble(&ok);
        param.expectedVoltage = ok ? voltage : 0.0;
    } else {
        param.expectedVoltage = 0.0;
    }

    if (index - 1 < ranges.size())
        param.loadMode = ranges[index - 1].trimmed();

    if (!param.levels.isEmpty())
        dcLoad->setDynamicCurrent(param);
}

void InstrumentExecutor::applyDyLoadSettings(
    DCLoad* dcLoad, int index, const QString& value,
    const QString& dyTime,
    const QVector<QString>& vons,
    const QVector<QString>& outputVoltages,
    const QVector<QString>& ranges)
{
    applyLoadVonSetting(dcLoad, index, vons);
    applyDyLoadValueSettings(dcLoad, index, value, dyTime, outputVoltages, ranges);
}

InstrumentExecutor::Result
InstrumentExecutor::runDyLoad(
    const Page1Config&             cfg,
    const QVector<DynamicDataRow>& rows,
    int                            conditionIndex,
    const DynamicMetaRow&          meta,
    DyLoadAction                   action,
    bool                           syncEnabled,
    bool                           syncDirty)
{
    using namespace InstrumentExecutorInternal;

    try {
        auto createResult = InstrumentCreator::createDCLoads(cfg, nullptr, TableKind::DyLoad);
        DCLoadSession loadSession(createResult);
        if (!createResult.success || createResult.dcLoads.isEmpty())
            return { false, "DC Load (DyLoad) creation failed" };

        MasterSlaveSyncPlan syncPlan;
        QString syncError;
        if (!buildSyncPlanIfNeeded(createResult.dcLoads,
                                   syncEnabled || syncDirty,
                                   syncPlan,
                                   syncError)) {
            return { false, syncError };
        }

        prepareDynamicLoadProgrammingIfNeeded(syncPlan, action);

        auto dataInfo = DataFinder::findDyLoadData(rows, conditionIndex, meta.t1t2);

        const QStringList failedChannels =
            action == DyLoadAction::DyloadOff
                ? QStringList{}
                : configureDynamicLoadChannels(createResult.dcLoads, dataInfo, meta);

        Result failureResult;
        if (handleDynamicLoadConfigurationFailure(action, failedChannels, syncPlan, failureResult))
            return failureResult;

        executeDynamicLoadOutputStep(createResult.dcLoads, syncPlan, action);

        return {};

    } catch (const std::exception& ex) {
        return { false, QString("[runDyLoad] Exception: %1").arg(ex.what()) };
    }
}
