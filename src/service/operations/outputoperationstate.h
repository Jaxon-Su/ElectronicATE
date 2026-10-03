#pragma once
#include "instrumentoperationresult.h"
#include "outputstate.h"
#include "page2config.h"
#include <QMap>
#include <QVector>

// Tracks settled output knowledge separately from queued writes that may change
// it.
class OutputOperationState
{
  public:
    using Channel = QPair<TableKind, int>;
    using Ticket = quint64;
    Ticket begin(QVector<Channel> channels, std::optional<bool> requested)
    {
        const auto ticket = ++m_next;
        m_pending.insert(ticket, {std::move(channels), requested});
        return ticket;
    }
    void complete(Ticket ticket, const InstrumentOperationResult &result)
    {
        if (!m_pending.contains(ticket))
            return;
        const auto operation = m_pending.take(ticket);
        for (const auto &channel : operation.channels)
        {
            if (result.confirmedOutput)
                m_settled[channel] = confirmedOutputState(*result.confirmedOutput);
            else if (result.success && operation.requested)
                m_settled[channel] = confirmedOutputState(*operation.requested);
            else if (!result.success && !result.outputUnchanged && operation.requested)
                m_settled[channel] = OutputState::Unknown;
        }
    }
    OutputState state(TableKind type, int source = 0) const
    {
        const Channel channel{type, source};
        for (const auto &operation : m_pending)
            if (operation.requested && operation.channels.contains(channel))
                return OutputState::Unknown;
        return m_settled.value(channel, OutputState::ConfirmedOff);
    }
    bool busy(TableKind type, int source = 0) const
    {
        for (const auto &operation : m_pending)
            if (operation.channels.contains({type, source}))
                return true;
        return false;
    }
    bool hasActiveControl() const
    {
        if (!m_pending.isEmpty())
            return true;
        for (auto state : m_settled)
            if (outputMayBeOn(state))
                return true;
        return false;
    }
    bool hasPendingOperations() const { return !m_pending.isEmpty(); }
    bool hasUnknownOutput() const
    {
        for (auto state : m_settled)
            if (state == OutputState::Unknown)
                return true;
        return false;
    }

  private:
    struct Operation
    {
        QVector<Channel> channels;
        std::optional<bool> requested;
    };
    QMap<Ticket, Operation> m_pending;
    QMap<Channel, OutputState> m_settled;
    Ticket m_next = 0;
};
