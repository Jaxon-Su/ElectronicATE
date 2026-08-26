#include "instrumentexecutor.h"

#include "dcload.h"

#include <QDebug>
#include <QHash>
#include <QRegularExpression>

namespace {

bool parseNumberWithOptionalUnit(QString text, QChar unit, double& value)
{
    text = text.trimmed();
    text.remove(QRegularExpression(R"(\s+)"));

    if (text.endsWith(unit, Qt::CaseInsensitive))
        text.chop(1);

    bool ok = false;
    value = text.toDouble(&ok);
    return ok;
}

bool parseCvDataValue(const QString& text, double& voltage, double& currentLimit)
{
    const QStringList parts = text.split('/', Qt::SkipEmptyParts);
    if (parts.size() != 2)
        return false;

    return parseNumberWithOptionalUnit(parts[0], 'V', voltage)
           && parseNumberWithOptionalUnit(parts[1], 'A', currentLimit);
}

bool configureDCLoad(
    DCLoad* dcLoad,
    int index,
    const DataFinder::LoadDataResult& dataInfo,
    const QVector<QString>& modes,
    const QVector<QString>& ranges,
    const QVector<QString>& vons,
    const QVector<QString>& outputVoltages)
{
    if (!dataInfo.found) {
        qWarning() << "[configureDCLoad] dataInfo not found, skip."
                   << "model=" << dcLoad->model()
                   << "CHAN=" << dcLoad->realChannel()
                   << "outputIndex=" << index;
        return false;
    }
    if (index <= 0 || index > dataInfo.values.size()) {
        qWarning() << "[configureDCLoad] outputIndex out of range:"
                   << "model=" << dcLoad->model()
                   << "CHAN=" << dcLoad->realChannel()
                   << "outputIndex=" << index
                   << "valuesSize=" << dataInfo.values.size();
        return false;
    }

    QString strValue = dataInfo.values[index - 1].trimmed();
    if (strValue.isEmpty()) {
        qWarning() << "[configureDCLoad] value is empty:"
                   << "model=" << dcLoad->model()
                   << "CHAN=" << dcLoad->realChannel()
                   << "outputIndex=" << index;
        return false;
    }

    QString mode = (index - 1 < modes.size()) ?
                       modes[index - 1].trimmed().toUpper() : "CC";
    double value = 0.0;
    double cvCurrentLimit = 0.0;
    bool hasCvCurrentLimit = false;

    if (mode == "CV") {
        hasCvCurrentLimit = parseCvDataValue(strValue, value, cvCurrentLimit);
        if (!hasCvCurrentLimit) {
            qWarning() << "[configureDCLoad] CV data value parse failed:"
                       << "model=" << dcLoad->model()
                       << "CHAN=" << dcLoad->realChannel()
                       << "outputIndex=" << index
                       << "raw=" << strValue
                       << "expected=\"voltage/current\", example=\"333V/3.16A\"";
            return false;
        }
    } else {
        const bool ok = parseNumberWithOptionalUnit(strValue, 'A', value);
        if (!ok) {
            qWarning() << "[configureDCLoad] value parse failed:"
                       << "model=" << dcLoad->model()
                       << "CHAN=" << dcLoad->realChannel()
                       << "outputIndex=" << index
                       << "raw=" << strValue;
            return false;
        }
    }

    int realindex = dcLoad->realChannel();
    qDebug() << "[configureDCLoad] model=" << dcLoad->model()
             << "CHAN=" << realindex
             << "outputIndex=" << index
             << "value=" << value
             << "mode=" << mode;

    dcLoad->setChannel(realindex);
    InstrumentExecutor::applyLoadSettings(
        dcLoad, index, value, mode, vons, outputVoltages, ranges, cvCurrentLimit, hasCvCurrentLimit);
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

QStringList configureStaticLoadChannels(
    const QVector<DCLoad*>& dcLoads,
    const DataFinder::LoadDataResult& dataInfo,
    const LoadMetaRow& meta)
{
    QStringList failedChannels;

    qDebug() << "[runLoad] Pass1 - configure all channels:";
    for (DCLoad* dcLoad : std::as_const(dcLoads)) {
        const bool configured =
            configureDCLoad(dcLoad, dcLoad->channelIndex(),
                            dataInfo, meta.modes, meta.ranges, meta.von, meta.vo);
        if (!configured)
            failedChannels << describeLoadChannel(dcLoad);
    }

    return failedChannels;
}

} // namespace InstrumentExecutorInternal

void InstrumentExecutor::applyLoadVonSetting(
    DCLoad* dcLoad, int index, const QVector<QString>& vons)
{
    if (index - 1 < vons.size()) {
        bool ok = false;
        double val = vons[index - 1].toDouble(&ok);
        if (ok) dcLoad->setVon(val);
    }
}

void InstrumentExecutor::applyLoadValueSettings(
    DCLoad* dcLoad, int index, double value,
    const QString& mode, const QVector<QString>& outputVoltages,
    const QVector<QString>& ranges,
    double cvCurrentLimit,
    bool hasCvCurrentLimit)
{
    int nSegments = dcLoad->getNumSegments();

    if (mode == "CV") {
        if (!hasCvCurrentLimit) {
            qWarning() << "[applyLoadValueSettings] CV current limit missing:"
                       << "model=" << dcLoad->model()
                       << "outputIndex=" << index;
            return;
        }

        const QString range = (index - 1 < ranges.size()) ? ranges[index - 1].trimmed() : QString();
        dcLoad->setCVSettings(value, cvCurrentLimit, range);
        return;
    }

    if (mode == "CR") {
        // TODO: implement CR
        return;
    }

    StaticCurrentParam param;
    param.levels      = QVector<double>(nSegments, value);
    param.enabledMask = QVector<bool>(nSegments, true);

    if (index - 1 < outputVoltages.size()) {
        bool ok;
        double voltage = outputVoltages[index - 1].toDouble(&ok);
        param.expectedVoltage = ok ? voltage : 0.0;
    } else {
        param.expectedVoltage = 0.0;
    }
    if (index - 1 < ranges.size())
        param.loadMode = ranges[index - 1].trimmed();

    dcLoad->setStaticCurrent(param);
}

void InstrumentExecutor::applyLoadSettings(
    DCLoad* dcLoad, int index, double value,
    const QString& mode,
    const QVector<QString>& vons,
    const QVector<QString>& outputVoltages,
    const QVector<QString>& ranges,
    double cvCurrentLimit,
    bool hasCvCurrentLimit)
{
    applyLoadVonSetting(dcLoad, index, vons);
    applyLoadValueSettings(dcLoad, index, value, mode, outputVoltages, ranges,
                           cvCurrentLimit, hasCvCurrentLimit);
}

InstrumentExecutor::Result
InstrumentExecutor::runLoad(
    const Page1Config&          cfg,
    const QVector<LoadDataRow>& rows,
    int                         conditionIndex,
    const LoadMetaRow&          meta,
    LoadAction                  action,
    bool                        syncEnabled)
{
    using namespace InstrumentExecutorInternal;

    try {
        auto createResult = InstrumentCreator::createDCLoads(cfg, nullptr, TableKind::Load);
        DCLoadSession loadSession(createResult);
        if (!createResult.success || createResult.dcLoads.isEmpty())
            return { false, "DC Load creation failed" };

        static const QHash<LoadAction, QString> kLoadActionName = {
            { LoadAction::LoadOn,  "LoadOn"  },
            { LoadAction::LoadOff, "LoadOff" },
            { LoadAction::Change,  "Change"  },
        };
        qDebug() << "[runLoad] action=" << kLoadActionName.value(action)
                 << "conditionIndex=" << conditionIndex
                 << "dcLoads=" << createResult.dcLoads.size();

        auto dataInfo = DataFinder::findLoadData(rows, conditionIndex);
        if (!dataInfo.found && action != LoadAction::LoadOff) {
            qWarning() << "[runLoad] Load data not found for conditionIndex=" << conditionIndex;
            return { false, "Load data not found for index " + QString::number(conditionIndex) };
        }

        MasterSlaveSyncPlan syncPlan;
        QString syncError;
        if (!buildSyncPlanIfNeeded(createResult.dcLoads,
                                   syncEnabled && (action == LoadAction::LoadOn || action == LoadAction::LoadOff),
                                   syncPlan,
                                   syncError)) {
            return { false, syncError };
        }

        prepareStaticLoadProgrammingIfNeeded(syncPlan, action);

        const QStringList failedChannels =
            action == LoadAction::LoadOff
                ? QStringList{}
                : configureStaticLoadChannels(createResult.dcLoads, dataInfo, meta);

        Result failureResult;
        if (handleStaticLoadConfigurationFailure(action, failedChannels, syncPlan, failureResult))
            return failureResult;

        executeStaticLoadOutputStep(createResult.dcLoads, syncPlan, action);

        return {};

    } catch (const std::exception& ex) {
        return { false, QString("[runLoad] Exception: %1").arg(ex.what()) };
    }
}
