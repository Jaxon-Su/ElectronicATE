#pragma once
#include <QString>
#include <QVector>
#include "page1config.h"
#include "page2config.h"
#include "datafinder.h"
#include "instrumentcreator.h"

// ══════════════════════════════════════════════════════
//  Action 枚舉（原定義於 page3viewmodel.h，移此共用）
//  Page3ViewModel 與 Page5TestWorker 都需要這些枚舉
// ══════════════════════════════════════════════════════
enum class InputAction  { PowerOn, PowerOff, Change };
enum class LoadAction   { LoadOn,  LoadOff,  Change };
enum class DyLoadAction { DyLoadOn, DyloadOff, Change };
enum class RelayAction  { RelayOn,  RelayOff,  Change };

class DCLoad;
class ACSource;
class RelayBase;

// ══════════════════════════════════════════════════════
//  InstrumentExecutor — 同步儀器執行服務（靜態類）
//
//  職責：
//    封裝 Input / Load / DyLoad / Relay 的同步硬體執行邏輯
//    供 Page3ViewModel（包在 QtConcurrent::run）與
//    Page5TestWorker（直接呼叫，已在 worker thread）共用
//
//  Result.success = false 時：
//    - 硬體建立/連線失敗 → MessageService 已顯示錯誤對話框
//    - 資料查找失敗       → errorMessage 有說明
//    呼叫方自行決定是否 emit forceOff / logMessage
// ══════════════════════════════════════════════════════
class InstrumentExecutor
{
public:
    struct Result {
        bool    success      = true;
        QString errorMessage;
    };

    // ── 主要執行方法 ──────────────────────────────────
    static Result runInput (const Page1Config&          cfg,
                            const QString&              inputText,
                            InputAction                 action);
    static Result runInput (const Page1Config&          cfg,
                            const InputRow&             inputRow,
                            InputAction                 action);

    static Result runLoad  (const Page1Config&          cfg,
                            const QVector<LoadDataRow>& rows,
                            int                         conditionIndex,
                            const LoadMetaRow&          meta,
                            LoadAction                  action,
                            bool                        syncEnabled = false);

    static Result runDyLoad(const Page1Config&              cfg,
                            const QVector<DynamicDataRow>&  rows,
                            int                             conditionIndex,
                            const DynamicMetaRow&           meta,
                            DyLoadAction                    action,
                            bool                            syncEnabled = false,
                            bool                            syncDirty   = false);

    static Result runRelay (const Page1Config&          cfg,
                            const QVector<RelayDataRow>& rows,
                            int                         conditionIndex,
                            RelayAction                 action);

    // ── apply* helpers（Page3ViewModel public API 代理來源）──
    static void applyLoadVonSetting    (DCLoad* dcLoad, int index,
                                        const QVector<QString>& vons);

    static void applyLoadValueSettings (DCLoad* dcLoad, int index,
                                        double value, const QString& mode,
                                        const QVector<QString>& outputVoltages,
                                        const QVector<QString>& ranges,
                                        double cvCurrentLimit = 0.0,
                                        bool hasCvCurrentLimit = false);

    static void applyLoadSettings      (DCLoad* dcLoad, int index,
                                        double value, const QString& mode,
                                        const QVector<QString>& vons,
                                        const QVector<QString>& outputVoltages,
                                        const QVector<QString>& ranges,
                                        double cvCurrentLimit = 0.0,
                                        bool hasCvCurrentLimit = false);

    static void applyDyLoadValueSettings(DCLoad* dcLoad, int index,
                                         const QString& value,
                                         const QString& dyTime,
                                         const QVector<QString>& outputVoltages,
                                         const QVector<QString>& ranges);

    static void applyDyLoadSettings    (DCLoad* dcLoad, int index,
                                        const QString& value,
                                        const QString& dyTime,
                                        const QVector<QString>& vons,
                                        const QVector<QString>& outputVoltages,
                                        const QVector<QString>& ranges);

private:
    InstrumentExecutor() = delete;
};

// Internal declarations shared by the split instrumentexecutor_*.cpp files.
// App code should call InstrumentExecutor only; these helpers are implementation details.
namespace InstrumentExecutorInternal {

constexpr int kSyncTypeSettleMs = 200;
constexpr int kSyncRunSettleMs = 300;
constexpr int kLoadOffSettleMs = 150;
constexpr int kDynamicRunSettleMs = 800;

struct SyncTypeSnapshot {
    DCLoad* load = nullptr;
    int type = -1;
};

struct MasterSlaveSyncPlan {
    bool valid = true;
    bool active = false;
    QString errorMessage;
    QList<SyncTypeSnapshot> snapshots;
    DCLoad* master = nullptr;
};

class DCLoadSession {
public:
    explicit DCLoadSession(InstrumentCreator::DCLoadResult& result);
    ~DCLoadSession();

    DCLoadSession(const DCLoadSession&) = delete;
    DCLoadSession& operator=(const DCLoadSession&) = delete;

private:
    InstrumentCreator::DCLoadResult& m_result;
};

class MasterSlaveSyncPlanner {
public:
    static MasterSlaveSyncPlan classify(const QList<DCLoad*>& dcLoads);
    static void setAll(const QList<SyncTypeSnapshot>& snapshots, int type);
    static void restore(const QList<SyncTypeSnapshot>& snapshots);
    static void sendLoadToIndependentMembers(
        const QList<DCLoad*>& dcLoads,
        const QList<SyncTypeSnapshot>& snapshots,
        bool on);

private:
    static QList<DCLoad*> devices(const QList<DCLoad*>& dcLoads);
    static bool capture(const QList<DCLoad*>& dcLoads, QList<SyncTypeSnapshot>& snapshots, QString& errorMessage);
    static void apply(const QList<SyncTypeSnapshot>& snapshots, int overrideType);
    static QString syncTypeName(int type);
    static int countType(const QList<SyncTypeSnapshot>& snapshots, int type);
    static DCLoad* master(const QList<SyncTypeSnapshot>& snapshots);
    static int typeFor(DCLoad* dcLoad, const QList<SyncTypeSnapshot>& snapshots);
};

bool buildSyncPlanIfNeeded(
    const QVector<DCLoad*>& dcLoads,
    bool shouldBuild,
    MasterSlaveSyncPlan& syncPlan,
    QString& errorMessage);

void prepareStaticLoadProgrammingIfNeeded(const MasterSlaveSyncPlan& syncPlan, LoadAction action);
void prepareDynamicLoadProgrammingIfNeeded(const MasterSlaveSyncPlan& syncPlan, DyLoadAction action);

bool handleStaticLoadConfigurationFailure(
    LoadAction action,
    const QStringList& failedChannels,
    const MasterSlaveSyncPlan& syncPlan,
    InstrumentExecutor::Result& result);

bool handleDynamicLoadConfigurationFailure(
    DyLoadAction action,
    const QStringList& failedChannels,
    const MasterSlaveSyncPlan& syncPlan,
    InstrumentExecutor::Result& result);

QStringList configureStaticLoadChannels(
    const QVector<DCLoad*>& dcLoads,
    const DataFinder::LoadDataResult& dataInfo,
    const LoadMetaRow& meta);

QStringList configureDynamicLoadChannels(
    const QVector<DCLoad*>& dcLoads,
    const DataFinder::DyLoadDataResult& dataInfo,
    const DynamicMetaRow& meta);

void executeStaticLoadOutputStep(
    const QVector<DCLoad*>& dcLoads,
    const MasterSlaveSyncPlan& syncPlan,
    LoadAction action);

void executeDynamicLoadOutputStep(
    const QVector<DCLoad*>& dcLoads,
    const MasterSlaveSyncPlan& syncPlan,
    DyLoadAction action);

void executeRelayAction(
    RelayBase* relay,
    RelayAction action,
    const InstrumentConfig& inst,
    const DataFinder::RelayDataResult& dataInfo);

} // namespace InstrumentExecutorInternal
