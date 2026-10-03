#pragma once
#include "capturedialog.h"
#include "delaydialogs/delaydialog.h"
#include "oscilloscopedialogs/createwriteoscilloscopedialog.h"
#include "page5conditionmapping.h"
#include "page5viewmodel.h"
#include "relaydialogs/relaydialog.h"
#include "stressdialogs/dynamicdialog.h"
#include "stressdialogs/staticdialog.h"
#include "turnonoffdialogs/shorttestdialog.h"
#include "turnonoffdialogs/turnonoffdialog.h"
#include <QMessageBox>
#include <memory>

// Owns one edit transaction. Only Accepted writes back to the stable task UID.
class TaskDialogEditor
{
  public:
    TaskDialogEditor(Page5ViewModel &vm, int uid, int sequence, QString extension, QWidget *parent)
        : m_vm(vm), m_uid(uid), m_sequence(sequence), m_extension(std::move(extension)), m_parent(parent),
          m_conditions(vm.executionContext())
    {
    }

    bool edit(const QString &task)
    {
        using Handler = std::function<bool()>;
        const QMap<QString, Handler> handlers{
            {"Capture",
             [this] {
                 return apply("capture", false, [this](const auto &cfg) {
                     return std::make_unique<CaptureDialog>(cfg, m_sequence, m_parent);
                 });
             }},
            {"Delay",
             [this] {
                 return apply("delay", false, [this](const auto &cfg) {
                     return std::make_unique<DelayDialog>(cfg, m_sequence, m_parent);
                 });
             }},
            {"Write Oscilloscope", [this] { return editScope(); }},
            {"Relay",
             [this] {
                 return apply("relay", true, [this](const auto &cfg) {
                     return std::make_unique<RelayDialog>(relays(), cfg, m_sequence, m_parent);
                 });
             }},
        };
        const auto kind = Page5TaskKind::fromName(task);
        if (Page5TaskKind::isMeasurement(task))
            return editMeasurement(kind);
        const auto handler = handlers.constFind(task);
        if (handler != handlers.cend())
            return (*handler)();
        QMessageBox::information(m_parent, task, "Settings are not available for this task.");
        return false;
    }

  private:
    template <class Builder> bool apply(const QString &key, bool referencesConditions, Builder build)
    {
        auto config = m_vm.taskConfig(m_uid, key);
        if (referencesConditions)
            Page5Conditions::prepareDialog(config, m_conditions);
        auto dialog = build(config);
        if (!dialog || dialog->exec() != QDialog::Accepted)
            return false;
        config = dialog->config();
        if (referencesConditions)
            Page5Conditions::capture(config, m_conditions);
        m_vm.setTaskConfig(m_uid, key, config);
        return true;
    }
    QStringList inputs() const { return Page5Conditions::inputOptions(m_conditions); }
    QStringList loads(bool dynamic) const
    {
        QStringList result;
        if (dynamic) {
            for (const auto &row : m_conditions.dynamics)
                result << Page5Conditions::rowOption(row.label, row.values);
        } else {
            for (const auto &row : m_conditions.loads)
                result << Page5Conditions::rowOption(row.label, row.values);
        }
        return result;
    }
    QVector<RelayOption> relays() const
    {
        QVector<RelayOption> result;
        for (const auto &row : m_conditions.relays)
            result.append({row.label, row.values});
        return result;
    }
    bool editMeasurement(Page5TaskKind::Kind kind)
    {
        using Kind = Page5TaskKind::Kind;
        const auto single = Page5TaskKind::singleKind(kind);
        const auto selection = Page5TaskKind::isGroup(kind) ? LoadConditionSelector::Selection::Group
                                                          : LoadConditionSelector::Selection::Single;
        const auto key = Page5TaskKind::settingsGroup(Page5TaskKind::name(kind));
        if (single == Kind::Static)
            return apply(key, true, [this, selection](const auto &cfg) {
                return std::make_unique<StaticDialog>(inputs(), loads(false), cfg, m_sequence, m_parent, selection);
            });
        if (single == Kind::Dynamic)
            return apply(key, true, [this, selection](const auto &cfg) {
                return std::make_unique<DynamicDialog>(inputs(), loads(true), cfg, m_sequence, m_parent, selection);
            });
        if (single == Kind::TurnOn || single == Kind::TurnOff)
            return apply(key, true, [this, single, selection](const auto &cfg) {
                return std::make_unique<TurnOnOffDialog>(single == Kind::TurnOn ? TurnOnOffDialog::TurnOn
                                                                              : TurnOnOffDialog::TurnOff,
                    inputs(), loads(false), relays(), cfg, m_sequence, m_parent, selection);
            });
        return apply(key, true, [this, single, selection](const auto &cfg) {
            return std::make_unique<ShortTestDialog>(single == Kind::ShortOn ? ShortTestDialog::ShortThenTurnOn
                                                                            : ShortTestDialog::TurnOnThenShort,
                inputs(), loads(false), relays(), cfg, m_sequence, m_parent, selection);
        });
    }
    bool editScope()
    {
        QString model;
        for (const auto &instrument : m_vm.page1Config().instruments)
            if (instrument.enabled && instrument.type.contains("Oscilloscope", Qt::CaseInsensitive)) {
                model = instrument.modelName;
                break;
            }
        return apply("osc", false, [this, model](const auto &cfg) {
            return std::unique_ptr<OscWriteDialogBase>(createWriteOscilloscopeDialog(
                m_vm.oscilloscope(), model, cfg, m_sequence, m_extension, m_parent));
        });
    }
    Page5ViewModel &m_vm;
    int m_uid;
    int m_sequence;
    QString m_extension;
    QWidget *m_parent;
    Page5ExecutionContext m_conditions;
};
