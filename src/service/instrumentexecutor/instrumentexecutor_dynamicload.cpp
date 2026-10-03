#include "instrumentexecutor.h"

#include "dcload.h"

#include <QDebug>
#include <cmath>
#include <stdexcept>

namespace
{

bool configureDCDyLoad(DCLoad *dcLoad, int index, const ConditionLookup::DynamicLoadDataResult &dataInfo,
                       const QVector<QString> &vons, const QVector<QString> &outputVoltages,
                       const QVector<QString> &ranges)
{
    if (!dataInfo.found) {
        qWarning() << "[configureDCDyLoad] dataInfo not found, skip."
                   << "model=" << dcLoad->model() << "CHAN=" << dcLoad->realChannel()
                   << "outputIndex=" << index;
        return false;
    }
    if (index <= 0 || index > dataInfo.values.size()) {
        qWarning() << "[configureDCDyLoad] outputIndex out of range:"
                   << "model=" << dcLoad->model() << "CHAN=" << dcLoad->realChannel()
                   << "outputIndex=" << index << "valuesSize=" << dataInfo.values.size();
        return false;
    }

    QString strValue = dataInfo.values[index - 1].trimmed();
    if (strValue.isEmpty()) {
        qWarning() << "[configureDCDyLoad] value is empty:"
                   << "model=" << dcLoad->model() << "CHAN=" << dcLoad->realChannel()
                   << "outputIndex=" << index;
        return false;
    }

    int realindex = dcLoad->realChannel();
    qDebug() << "[configureDCDyLoad] model=" << dcLoad->model() << "CHAN=" << realindex
             << "outputIndex=" << index << "value=" << strValue << "t1t2=" << dataInfo.t1t2;

    dcLoad->setChannel(realindex);
    InstrumentExecutor::applyDyLoadSettings(dcLoad, index, strValue, dataInfo.t1t2, vons, outputVoltages,
                                            ranges);
    return true;
}

QString describeLoadChannel(DCLoad *dcLoad)
{
    if (!dcLoad)
        return {};

    return QString("%1 CHAN %2 outputIndex %3")
        .arg(dcLoad->model())
        .arg(dcLoad->realChannel())
        .arg(dcLoad->channelIndex());
}

} // namespace

namespace InstrumentExecutorInternal
{

QStringList configureDynamicLoadChannels(const QVector<DCLoad *> &dcLoads,
                                         const ConditionLookup::DynamicLoadDataResult &dataInfo,
                                         const DynamicMetaRow &meta)
{
    QStringList failedChannels;

    for (DCLoad *dcLoad : std::as_const(dcLoads)) {
        const bool configured =
            configureDCDyLoad(dcLoad, dcLoad->channelIndex(), dataInfo, meta.von, meta.vo, meta.ranges);
        if (!configured)
            failedChannels << describeLoadChannel(dcLoad);
    }

    return failedChannels;
}

} // namespace InstrumentExecutorInternal

void InstrumentExecutor::applyDyLoadValueSettings(DCLoad *dcLoad, int index, const QString &value,
                                                  const QString &dyTime,
                                                  const QVector<QString> &outputVoltages,
                                                  const QVector<QString> &ranges)
{
    if (!dcLoad || index <= 0)
        throw std::invalid_argument("Dynamic load requires a valid device and positive output index");

    int nSegments = dcLoad->getNumSegments();
    DynamicCurrentParam param;

    const QStringList currentParts = value.split('~');
    if (nSegments < 1 || currentParts.size() > nSegments)
        throw std::invalid_argument("Unsupported dynamic current segment count");
    for (const auto &part : currentParts) {
        bool ok = false;
        double v = part.trimmed().toDouble(&ok);
        if (!ok || !std::isfinite(v) || v < 0)
            throw std::invalid_argument("Dynamic current must contain finite, non-negative values");
        param.levels.append(v);
        param.enabledMask.append(true);
    }
    if (param.levels.size() == 1 && nSegments >= 2) {
        param.levels.append(param.levels[0]);
        param.enabledMask.append(true);
    }

    const QStringList timeParts = dyTime.split('~');
    if (timeParts.size() > 2)
        throw std::invalid_argument("Dynamic timing requires one or two durations");
    for (const auto &part : timeParts) {
        bool ok = false;
        double v = part.trimmed().toDouble(&ok);
        if (!ok || !std::isfinite(v) || v <= 0 || v / 1000.0 <= 0)
            throw std::invalid_argument("Dynamic durations must be finite and positive (ms)");
        param.timings.append(v / 1000.0);
    }
    if (param.timings.size() == 1)
        param.timings.append(param.timings[0]);

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

void InstrumentExecutor::applyDyLoadSettings(DCLoad *dcLoad, int index, const QString &value,
                                             const QString &dyTime, const QVector<QString> &vons,
                                             const QVector<QString> &outputVoltages,
                                             const QVector<QString> &ranges)
{
    applyLoadVonSetting(dcLoad, index, vons);
    applyDyLoadValueSettings(dcLoad, index, value, dyTime, outputVoltages, ranges);
}

InstrumentExecutor::Result InstrumentExecutor::runDynamicLoad(const Page1Config &cfg,
                                                         const QVector<DynamicDataRow> &rows,
                                                         int conditionIndex, const DynamicMetaRow &meta,
                                                         DynamicLoadAction action, bool syncEnabled,
                                                         bool syncDirty)
{
    using namespace InstrumentExecutorInternal;

    bool outputUnchanged = true;
    try {
        auto createResult = InstrumentCreator::createDCLoads(cfg, TableKind::DyLoad);
        if (!createResult.success || createResult.dcLoads().isEmpty())
            return {false, createResult.errorMessage.isEmpty() ? "DC Load (DyLoad) creation failed"
                                                               : createResult.errorMessage, true};
        if (action == DynamicLoadAction::LoadOff) {
            outputUnchanged = false;
            executeDynamicLoadOutputStep(createResult.dcLoads(), {}, action);
            return {};
        }

        MasterSlaveSyncPlan syncPlan;
        QString syncError;
        if (!buildSyncPlanIfNeeded(createResult.dcLoads(), syncEnabled || syncDirty, syncPlan, syncError)) {
            return {false, syncError, true};
        }

        outputUnchanged = false;
        prepareDynamicLoadProgrammingIfNeeded(syncPlan, action);

        auto dataInfo = ConditionLookup::findDynamicLoadData(rows, conditionIndex, meta.t1t2);

        const QStringList failedChannels =
            action == DynamicLoadAction::LoadOff
                ? QStringList{}
                : configureDynamicLoadChannels(createResult.dcLoads(), dataInfo, meta);

        Result failureResult;
        if (handleDynamicLoadConfigurationFailure(action, failedChannels, syncPlan, failureResult))
            return failureResult;

        executeDynamicLoadOutputStep(createResult.dcLoads(), syncPlan, action);

        return {};

    } catch (const std::exception &ex) {
        return {false, QString("[runDynamicLoad] Exception: %1").arg(ex.what()), outputUnchanged};
    }
}
